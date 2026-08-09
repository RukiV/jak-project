;; og:preserve-this bring-up: the icew bsp entry converted to a .go raw copy (brdroom
;; recipe, same as jungles.gd). The three .o entries are real code: peak-part.gc is the
;; live icelands mood and particle code (the sole caller of update-mood-flames, #103);
;; peak-obs.gc and ice-obs.gc are empty stubs that compile and intern nothing. icew rides
;; every icelands continue want-set, so the track's code loads before any section bsp,
;; satisfying the #51 ordering discipline natively (no reorder was needed here).
("ICEW.DGO"
 ("peak-part.o"
  "peak-obs.o"
  "ice-obs.o"
  "icew.go"
 ))
