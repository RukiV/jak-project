;; og:preserve-this bring-up: havjungx-vis converted to a .go raw copy (brdroom recipe,
;; same as havjungw.gd/havjungs.gd). The nine .o entries are the same real-code objects
;; havjungw.gd carries (havjung-part.gc, havjung-ocean.gc, jungle-part.gc, jungle-obs.gc,
;; jungle-effects.gc, havjung-effects.gc, haven-part.gc, haven-obs.gc,
;; construction-obs.gc); havjungw.gd's note records which of those are live data vs
;; empty 9-line stubs. jungle-obs.o must stay armed here for the same reason jgx.gd
;; documents: the eight jungle-* art groups this DGO also carries lean on its prop
;; deftypes, and want-set discipline (not DGO-internal order) is what keeps a
;; section-first load from interning a stub ahead of it (forge #51,
;; game/kernel/jakx/kscheme.cpp).
("HJX.DGO"
 ("havjung-part.o"
  "havjung-ocean.o"
  "jungle-part.o"
  "jungle-obs.o"
  "jungle-effects.o"
  "havjung-effects.o"
  "haven-part.o"
  "haven-obs.o"
  "construction-obs.o"
  "tpage-1861.go"
  "tpage-1864.go"
  "tpage-1880.go"
  "tpage-1863.go"
  "jungle-debris-jar-a-ag.go"
  "jungle-debris-jar-b-ag.go"
  "jungle-debris-ag.go"
  "jungle-clay-jar-b-ag.go"
  "jungle-clay-jar-a-ag.go"
  "jungle-tree-root-large-b-ag.go"
  "jungle-tree-root-large-a-ag.go"
  "jungle-flaming-arrow-ag.go"
  "havjungx-vis.go"
 ))
