;;;; Abuse: Vrenna, challenge pack.
;;;;
;;;; SKELETON. There is no map here yet, and loading this add-on today drops
;;;; you into the first campaign level. It is committed empty so that the
;;;; layout, the numbering and the manifest entry are settled before any
;;;; content arrives.
;;;;
;;;;   abuse-vrenna -a challenge
;;;;
;;;; loads this file, which is the whole interface the add-on system has.
;;;;
;;;; Standalone maps, each one finished in a sitting, with a timer and a best
;;;; time instead of a campaign to carry on. Nothing here touches the
;;;; campaign, the engine or data/lisp/: an add-on is a separate world by
;;;; construction, which is why the pack can exist at all.
;;;;
;;;; Tile numbering, inherited from the original add-on rules: foreground
;;;; tiles start at 1200 and background tiles at 350, because the base game
;;;; uses everything below that. Every image must carry the foretile or
;;;; backtile type or the loader ignores it.
;;;;
;;;; The plan, the map catalogue and the menu design are in
;;;; docs/plan/challenge-maps.md, which is local and not in the repository.

;; Tiles of our own. Nothing to load yet: the first maps are built from the
;; tiles the game already has, on purpose, to prove the pipeline before
;; anyone spends effort on art.
;;
;; (load_tiles "addon/challenge/challenge.spe")

;; The map to start on. One so far, compiled by tools/asciimap from the text
;; grid in tools/asciimap/maps/distress.txt.
(set_first_level "addon/challenge/maps/distress.lvl")

;; The rest of the game, unchanged.
(load "abuse.lsp")
