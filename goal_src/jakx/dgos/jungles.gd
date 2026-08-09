;; og:preserve-this bring-up: data entries converted to .go raw copies (brdroom recipe,
;; same as junglew.gd). This DGO is pure data: five tpages (2333 is jungles-sprite, the
;; jungle track's sprite page, #101), three prop art groups and the bsp. The prop
;; deftypes these art groups lean on are interned by jungle-obs.o, which the want-set
;; discipline loads first at every jungle continue point (junglew/jgx carry it); see
;; the forge #51 note in game/kernel/jakx/kscheme.cpp.
("JUNGLES.DGO"
 ("tpage-2375.go"
  "tpage-1682.go"
  "tpage-2563.go"
  "tpage-2333.go"
  "tpage-1847.go"
  "jungle-stone-snake-head-ag.go"
  "jungle-statue-small-outdoors-ag.go"
  "jungle-statue-small-debris-ag.go"
  "jungles.go"
 ))
