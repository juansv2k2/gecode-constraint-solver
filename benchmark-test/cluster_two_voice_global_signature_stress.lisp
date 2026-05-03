(defpackage :iterate 
    (:use :cl))
(ignore-errors 
    (require :sb-introspect))

(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/package.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/01.domain.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/02.engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/03.Fwd-rules.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/04.Backtrack-rules.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05.rules-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05a.rules-interface-1engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05b.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05c.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05d.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05d-2.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05e.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05f.rules-interface-3engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05g.rules-interface-any-n-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05h.rules-higher-level.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05i.rules-stop-search.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05n.rules-interface-nn-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05o.new-canon-rule.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06.heuristic-rules-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06a.heuristic-rules-interface-1engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06b.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06c.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06d.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06e.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06f.heuristic-rules-interface-3engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06g.heuristic-rules-interface-any-n-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06h.heuristic-rules-interface-added2020.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/07.backjumping.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/08.decode.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/09.utilities.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/_000.main-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/from-studio-flat.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/export.lisp")

(in-package :cluster-engine)

(setf *max-nr-of-loops* 300000000)

(defun all-diff-list? 
    (xs)
    (if 
        (null xs) t
        (and 
            (not 
                (member 
                    (car xs) 
                    (cdr xs) :test #'equal))
            (all-diff-list? 
                (cdr xs)))))

(defun no-unison? 
    (sim)
    (or 
        (null 
            (first sim))
        (null 
            (second sim))
        (/= 
            (first sim) 
            (second sim))))

;;; Global order-signature constraint.
;;; It is intentionally evaluated only when the full 12-note line is present,
;;; making it a late-pruning rule that increases backtracking pressure.
(defun interval-signature-mod53? 
    (xs)
    (if 
        (< 
            (length xs) 12)
t
        (let 
            (
                (score 0))
            (loop for i from 0 to 10 do
                (incf score 
                    (* 
                        (1+ i)
                        (abs 
                            (- 
                                (nth 
                                    (1+ i) xs)
                                (nth i xs))))))
            (= 
                (mod score 53) 13))))

(defparameter *voices* '
    (0 1))
(defparameter *pitch-domain* '
    (
        (60) 
        (61) 
        (62) 
        (63) 
        (64) 
        (65) 
        (66) 
        (67) 
        (68) 
        (69) 
        (70) 
        (71)))
(defparameter *rhythm-domain* '
    (
        (1/4)))
(defparameter *domains* 
    (list *rhythm-domain* *pitch-domain* *rhythm-domain* *pitch-domain*))

(defparameter *rules*
    (Rules->Cluster
;; Twelve-tone constraint per voice
        (R-pitches-one-voice #'all-diff-list? *voices* :all-pitches)
;; Stress rule: late global order signature per voice
        (R-pitches-one-voice #'interval-signature-mod53? *voices* :all-pitches)
;; Cross-voice local constraint
        (R-pitch-pitch #'no-unison? '
            (0 1) nil :all :exclude-gracenotes :pitch)))

(handler-case
    (sb-ext:with-timeout 60
        (let 
            (
                (res 
                    (time 
                        (ClusterEngine 12 t nil *rules* '
                            (
                                (4 4)) *domains*))))
            (format t "~%RESULT-OK: ~A~%" 
                (if res t nil))
            (when res
                (format t "Voice 0: ~A~%" 
                    (nth 1 res))
                (format t "Voice 1: ~A~%" 
                    (nth 3 res)))))
    (sb-ext:timeout 
        ()
        (format t "~%RESULT-OK: TIMEOUT (60s)~%")))

(quit)
