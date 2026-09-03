QITN / Matek Micro BEC KiCad library

Files:
- QITN_Matek_Micro_BEC.kicad_sym
- QITN_Matek_Micro_BEC.pretty/BEC_QITN_Matek_Micro_18x16mm_Castellated.kicad_mod

Symbol pin order:
1 OUT
2 GND
3 GND
4 IN

Footprint assumptions:
- Board approx 18 mm x 16 mm.
- Four castellated pads on one edge at 2.54 mm pitch.
- Component/top side up, pads on LEFT edge:
  top to bottom = OUT, GND, GND, IN.
- Pad size is estimated from product photos (2.3 x 1.45 mm). Verify and adjust for your exact board before ordering a PCB.

Important:
- This is a module footprint/symbol, not the internal switching-regulator schematic.
- Set the output voltage by soldering the 5V/9V/12V selector pads on the module itself before installing.
- No reverse input polarity protection according to Matek's page.
