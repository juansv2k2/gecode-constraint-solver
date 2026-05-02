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

(setf *max-nr-of-loops* 150000000)

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

(defun retro-pair?
    (a b)
    (=
        (first a)
        (second b)))

(defun inversion-pair?
    (a b)
    (=
        (second b)
        (- 131
            (first a))))

(defparameter *voices* '
    (0))
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
    (list
*rhythm-domain**pitch-domain*
*rhythm-domain**pitch-domain*
*rhythm-domain**pitch-domain*
*rhythm-domain**pitch-domain*))

(defparameter *rules*
    (apply #'Rules->Cluster
        (append
            (list
                (R-pitches-one-voice #'all-diff-list? *voices* :all-pitches))
            (loop for i from 0 to 11
collect
                (R-pitch-pitch #'retro-pair?
'
                    (0 1)
                    (list 
                        (/ i 4) 
                        (/ 
                            (- 11 i) 4))
:at-timepoints:exclude-gracenotes:pitch))
            (loop for i from 0 to 11
collect
                (R-pitch-pitch #'inversion-pair?
'
                    (0 2)
                    (list 
                        (/ i 4) 
                        (/ i 4))
:at-timepoints:exclude-gracenotes:pitch))
            (loop for i from 0 to 11
collect
                (R-pitch-pitch #'inversion-pair?
'
                    (0 3)
                    (list 
                        (/ i 4) 
                        (/ 
                            (- 11 i) 4))
:at-timepoints:exclude-gracenotes:pitch)))))

(let
    (
        (res
            (time
                (ClusterEngine 12 t nil *rules* '
                    (
                        (4 4)) *domains*))))
    (format t "~%RESULT-OK: ~A~%"
        (if res t nil)))

(quit)
