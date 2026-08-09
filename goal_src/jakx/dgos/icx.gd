;; og:preserve-this bring-up: icex-vis converted to a .go raw copy (brdroom recipe, same
;; as jungles.gd). Dormant cross-track trap, recorded per #112: ice-flag has no deftype
;; anywhere (commented out at decompiler/config/jakx/all-types.gc:61796) and ice-obs.gc
;; is an empty stub, so the section bsps intern ice-flag as a 12-method stub at load,
;; harmless while nothing widens it. The icepass track's want-set loads icea through
;; icepassw.gd, which carries no ice-obs object at all, so the moment a real ice-flag
;; deftype lands in ice-obs.gc, the icepass path interns the stub first and the real
;; deftype then hits the kscheme widen assert (the forge #51 disease). Whoever lands
;; ice-flag must arm ice-obs in icepassw.gd in the same change.
("ICX.DGO"
 ("peak-part.o"
  "peak-obs.o"
  "ice-obs.o"
  "tpage-2719.go"
  "tpage-3120.go"
  "ice-flag-ag.go"
  "icex-vis.go"
 ))
