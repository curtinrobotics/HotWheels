
# Controller ESP-32 Communication

1. Add esp-idf to nix file
 ```bash
# flake.nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs = { self, nixpkgs, esp-dev }:
    let
      system = "x86_64-linux"; # or aarch64-darwin, etc.
      pkgs = import nixpkgs {
        inherit system;
        overlays = [ esp-dev.overlays.default ];
      };
    in {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [
          pkgs.esp-idf-full   # includes idf.py, cmake, ninja, toolchains
        ];
        shellHook = ''
          export IDF_PATH="${pkgs.esp-idf-full}"
        '';
      };
    };
}
 
 ```
Run nix deveand confirm with idf.py --version. This sidesteps the usual install.sh / export.sh dance since Nix wires up the toolchain paths for you.

(If that flake's ESP-IDF version is older than you want, alternative: use Nix only for system deps — cmake, ninja, python, the Xtensa toolchain package — and clone ESP-IDF yourself, running its own install.sh/export.sh inside the Nix shell. More manual, but decouples you from the flake's release cadence.)

```bash
idf.py create-project dualsense_host
cd dualsense_host
idf.py set-target esp32s3
```

## Pull in the HID host component

ESP-IDF's low-level usb_host lib is built in, but the HID class driver (usb_host_hid) is a managed component from the component registry, not part of core IDF. Add it via a component manifest:

```yaml
# main/idf_component.yml
dependencies:
  espressif/usb_host_hid: "*"
  idf: ">=5.0"
```
Then idf.py reconfigure — it'll fetch the component automatically (needs network access; if your Nix sandbox blocks it, run this step outside the pure sandbox or vendor the component locally).

Run idf.py menuconfig and check:

- Component config → USB Host — should already be enabled for S3 target; bump Max number of supported devices / transfer buffer sizes if defaults feel tight.
- You do not want the TinyUSB/CDC device-mode options here — that's for when the ESP32-S3 acts as a USB device, not a host. Host mode uses the separate usb_host driver.
- Confirm CONFIG_IDF_TARGET_ESP32S3=y in sdkconfig.

2. Initialise usb host driver
- `idf.py menuconfig`
    - Component config -> USB Stack -> Enable USB Host functionality
3. 


# Controller Mapping
📊 Mapping the PS5 Raw HID Packet StructureBy default, when a PS5 controller connects via wire, it sends 64-byte input reports (Report ID Over USB, the DualSense sends a 64-byte input report (report ID 0x01) containing:

Left/right stick X/Y (bytes 1–4)
L2/R2 analog trigger values (bytes 5–6)
A counter byte
Buttons packed into bits (face buttons, D-pad as a 4-bit hat, shoulder buttons, stick clicks, share/options/PS/touchpad-click)
Trigger feedback status, battery, gyro/accelerometer (6-axis, 2 bytes each), and touchpad finger coordinates further in the buffer

# Build/Flash

```bash
idf.py build
idf.py -p /dev/ttyACMO flash monitor


```cpp
/*
  ESP-IDF USB Host: PS5 DualSense controller reader
  ----------------------------------------------------
  Target: ESP32-S3
  Requires component: espressif/usb_host_hid (add to main/idf_component.yml)

  This follows the standard ESP-IDF usb_host + hid_host driver pattern:
    - usb_host library install + event-handling task
    - hid_host driver install + event-handling task
    - app-level event queue: the HID driver's device callback runs in the
      driver's context, so it just posts an event; app_main's loop does the
      actual work (opening devices, starting them, reading reports).

  NOTE: API names/signatures match ESP-IDF v5.x + usb_host_hid managed
  component as of this writing. If your resolved component version differs,
  check usb/hid_host.h and hid_usage_keyboard.h in your local
  managed_components/espressif__usb_host_hid/include for exact signatures —
  minor field/enum renames happen between component releases.

  Build:
    idf.py set-target esp32s3
    idf.py build flash monitor
*/

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "dualsense";

/* ---------- App event plumbing ---------- */

typedef enum {
  APP_EVENT_HID_HOST,
} app_event_group_t;

typedef struct {
  app_event_group_t event_group;
  struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
    void *arg;
  } hid_host_device;
} app_event_queue_t;

static QueueHandle_t app_event_queue = NULL;

/* ---------- DualSense report parsing ---------- */

static void parse_dualsense_report(const uint8_t *data, size_t len) {
  // Wired USB report: report ID 0x01, 64 bytes total.
  if (len < 10 || data[0] != 0x01) {
    return;
  }

  uint8_t lx = data[1];
  uint8_t ly = data[2];
  uint8_t rx = data[3];
  uint8_t ry = data[4];
  uint8_t l2 = data[5];
  uint8_t r2 = data[6];

  uint8_t dpad = data[8] & 0x0F;
  bool square = data[8] & 0x10;
  bool cross = data[8] & 0x20;
  bool circle = data[8] & 0x40;
  bool triangle = data[8] & 0x80;

  bool l1 = data[9] & 0x01;
  bool r1 = data[9] & 0x02;
  bool share = data[9] & 0x10;
  bool options = data[9] & 0x20;
  bool l3 = data[9] & 0x40;
  bool r3 = data[9] & 0x80;

  bool ps = (len > 10) && (data[10] & 0x01);
  bool touchpad = (len > 10) && (data[10] & 0x02);

  ESP_LOGI(TAG,
           "LX:%3d LY:%3d RX:%3d RY:%3d L2:%3d R2:%3d DPad:%d "
           "%s%s%s%s%s%s%s%s%s%s",
           lx, ly, rx, ry, l2, r2, dpad, square ? "SQ " : "", cross ? "X " : "",
           circle ? "O " : "", triangle ? "TRI " : "", l1 ? "L1 " : "",
           r1 ? "R1 " : "", share ? "SHR " : "", options ? "OPT " : "",
           ps ? "PS " : "", touchpad ? "TP " : "");
  (void)l3;
  (void)r3;
}

/* ---------- HID interface (report) callback ---------- */

static void
hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                            const hid_host_interface_event_t event, void *arg) {
  uint8_t data[64] = {0};
  size_t data_length = 0;
  hid_host_dev_params_t dev_params;

  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
  case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
    ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
        hid_device_handle, data, sizeof(data), &data_length));

    if (dev_params.sub_class == HID_SUBCLASS_NO_SUBCLASS) {
      parse_dualsense_report(data, data_length);
    }
    break;

  case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "HID interface disconnected");
    ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
    break;

  case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
    ESP_LOGE(TAG, "HID transfer error");
    break;

  default:
    break;
  }
}

/* ---------- HID driver (device connect/disconnect) callback ---------- */

static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                                     const hid_host_driver_event_t event,
                                     void *arg) {
  // Runs in the driver's own context — just forward to app_main via queue.
  app_event_queue_t evt_queue = {
      .event_group = APP_EVENT_HID_HOST,
      .hid_host_device.handle = hid_device_handle,
      .hid_host_device.event = event,
      .hid_host_device.arg = arg,
  };
  xQueueSend(app_event_queue, &evt_queue, 0);
}

/* ---------- USB Host lib event task ---------- */

static void usb_lib_task(void *arg) {
  while (1) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      ESP_LOGI(TAG, "USB: all devices freed");
    }
  }
}

/* ---------- HID host driver event task ---------- */

static void hid_host_device_handle_event(app_event_queue_t *evt) {
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(
      hid_host_device_get_params(evt->hid_host_device.handle, &dev_params));

  switch (evt->hid_host_device.event) {
  case HID_HOST_DRIVER_EVENT_CONNECTED: {
    ESP_LOGI(TAG, "HID device connected, iface=%d subclass=%d proto=%d",
             dev_params.iface_num, dev_params.sub_class, dev_params.proto);

    const hid_host_device_config_t dev_config = {
        .callback = hid_host_interface_callback,
        .callback_arg = NULL,
    };

    ESP_ERROR_CHECK(
        hid_host_device_open(evt->hid_host_device.handle, &dev_config));
    ESP_ERROR_CHECK(
        hid_class_request_set_idle(evt->hid_host_device.handle, 0, 0));
    ESP_ERROR_CHECK(hid_host_device_start(evt->hid_host_device.handle));
    break;
  }
  default:
    break;
  }
}

static void hid_host_task(void *arg) {
  app_event_queue_t evt_queue;
  while (1) {
    if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
      if (evt_queue.event_group == APP_EVENT_HID_HOST) {
        hid_host_device_handle_event(&evt_queue);
      }
    }
  }
}

/* ---------- app_main ---------- */

void app_main(void) {
  app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

  // 1. Install the USB Host library
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  ESP_ERROR_CHECK(usb_host_install(&host_config));

  // Task that pumps usb_host library events (device connect/disconnect at
  // the USB level, distinct from HID-level events below).
  xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 6, NULL);

  // 2. Install the HID host driver on top of usb_host
  const hid_host_driver_config_t hid_host_driver_config = {
      .create_background_task = true,
      .task_priority = 5,
      .stack_size = 4096,
      .core_id = 0,
      .callback = hid_host_device_callback,
      .callback_arg = NULL,
  };
  ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

  // Task that consumes our app-level queue (device connected -> open/start)
  xTaskCreate(hid_host_task, "hid_host_task", 4096, NULL, 5, NULL);

  ESP_LOGI(TAG, "USB Host + HID driver ready. Plug in the DualSense...");
}
```