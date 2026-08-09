;; og:preserve-this bring-up: the havjungw bsp entry converted to a .go raw copy
;; (brdroom recipe, same as jungles.gd). The nine .o entries are real code compiling
;; from goal_src: jungle-part.gc, jungle-obs.gc, jungle-effects.gc and
;; havjung-effects.gc are the same jungle-family objects junglew.gd/jgx.gd carry;
;; havjung-ocean.gc and haven-part.gc hold substantial havjung/haven-specific data
;; (an ocean color table, the group-haven-gen-light particle group). havjung-part.gc,
;; haven-obs.gc and construction-obs.gc are empty 9-line stubs that compile and
;; intern nothing, the same footing as icew.gd's peak-obs/ice-obs; their file
;; headers list several other unarmed DGOs (HAVENW, HAVSEWW, HAVTOURW, HJX, HSX,
;; HVX and more), so landing real content there is a multi-DGO change, not local to
;; havjungw. jungle-obs.o carries the jungle-stone-snake-head prop deftype family
;; that havjungs.gd's shared art leans on; this rung does not boot, so whether
;; havjungw's want-set actually loads ahead of havjungs per the #51 discipline
;; (game/kernel/jakx/kscheme.cpp) is unverified here (issue #129 step 5).
("HAVJUNGW.DGO"
 ("havjung-part.o"
  "havjung-ocean.o"
  "jungle-part.o"
  "jungle-obs.o"
  "jungle-effects.o"
  "havjung-effects.o"
  "haven-part.o"
  "haven-obs.o"
  "construction-obs.o"
  "tpage-1635.go"
  "jungle-flaming-arrow-ag.go"
  "havjungw.go"
 ))
