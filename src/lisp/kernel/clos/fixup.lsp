;; Should be commented out
#+(or)
(eval-when (:execute)
	(format t "!~%!~%!~%!~%!~%In fixup.lsp !~%  Turning on :compare *feature*  for ensure-generic-function~%!~%!~%!~%!~%")
	(setq core:*echo-repl-read* t)
	(setq cl:*features* (cons :compare cl:*features*)))

;;;;  -*- Mode: Lisp; Syntax: Common-Lisp; Package: CLOS -*-
;;;;
;;;;  Copyright (c) 1992, Giuseppe Attardi.
;;;;  Copyright (c) 2001, Juan Jose Garcia Ripoll.
;;;;
;;;;    This program is free software; you can redistribute it and/or
;;;;    modify it under the terms of the GNU Library General Public
;;;;    License as published by the Free Software Foundation; either
;;;;    version 2 of the License, or (at your option) any later version.
;;;;
;;;;    See file '../Copyright' for full details.

(in-package "CLOS")
#+(or)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (setf *echo-repl-read* t))

#+clasp
(defun subclasses* (class)
  (remove-duplicates
   (cons class
         (mapappend #'subclasses*
                    (class-direct-subclasses class)))))

;;; ----------------------------------------------------------------------
;;;                                                                  slots

#|
(defclass effective-slot-definition (slot-definition))

(defclass direct-slot-definition (slot-definition))

(defclass standard-slot-definition (slot-definition))

(defclass standard-direct-slot-definition (standard-slot-definition direct-slot-definition))

(defclass standard-effective-slot-definition (standard-slot-definition direct-slot-definition))
|#


;;; This will print every form as its compiled
#+(or)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (format t "Starting fixup.lsp")
  (setq *echo-repl-tpl-read* t)
  (setq *load-print* t)
  (setq *echo-repl-read* t))


(defmethod reader-method-class ((class std-class)
				(direct-slot direct-slot-definition)
				&rest initargs)
  (declare (ignore class direct-slot initargs))
  (find-class (if (member (class-name (class-of class))
			  '(standard-class
			    funcallable-standard-class
			    structure-class))
		  'standard-optimized-reader-method
		  'standard-reader-method)))


(defmethod writer-method-class ((class std-class)
				(direct-slot direct-slot-definition)
				&rest initargs)
  (declare (ignore class direct-slot initargs))
  (find-class (if (member (class-name (class-of class))
			  '(standard-class
			    funcallable-standard-class
			    structure-class))
		  'standard-optimized-writer-method
		  'standard-reader-method)))

;;; ----------------------------------------------------------------------
;;; Fixup
(defun register-method-with-specializers (method)
  (declare (si::c-local))
  (loop for spec in (method-specializers method)
     do (add-direct-method spec method)))


(dolist (method-info *early-methods* (makunbound '*EARLY-METHODS*))
  (let* ((method-name (car method-info))
	 (gfun (fdefinition method-name))
	 (standard-method-class (find-class 'standard-method)))
    (when (eq 'T (class-id (si:instance-class gfun)))
      ;; complete the generic function object
      (si:instance-class-set gfun (find-class 'STANDARD-GENERIC-FUNCTION))
      (si::instance-sig-set gfun)
      (setf (slot-value gfun 'method-class) standard-method-class)
      (setf (slot-value gfun 'docstring) nil)
      )
    (dolist (method (cdr method-info))
      ;; complete the method object
      (let ((old-class (si::instance-class method)))
	(si::instance-class-set method
				(cond ((null old-class)
				       (find-class 'standard-method))
				      ((symbolp old-class)
				       (find-class (truly-the symbol old-class)))
				      (t
				       old-class))))
      (si::instance-sig-set gfun)
      (register-method-with-specializers method)
      )
    ))



;;; ----------------------------------------------------------------------
;;;                                                              redefined

(defun method-p (method) (typep method 'METHOD))

(defun make-method (method-class qualifiers specializers arglist function options)
  (apply #'make-instance
	 method-class
	 :generic-function nil
	 :qualifiers qualifiers
	 :lambda-list arglist
	 :specializers specializers
	 :function function
	 :allow-other-keys t
	 options))

(defun all-keywords (l)
  (declare (si::c-local))
  (let ((all-keys '()))
    (do ((l (rest l) (cddddr l)))
	((null l)
	 all-keys)
      (push (first l) all-keys))))

(defun congruent-lambda-p (l1 l2)
  (multiple-value-bind (r1 opts1 rest1 key-flag1 keywords1 a-o-k1)
      (si::process-lambda-list l1 'FUNCTION)
    (declare (ignore a-o-k1))
    (multiple-value-bind (r2 opts2 rest2 key-flag2 keywords2 a-o-k2)
	(si::process-lambda-list l2 'FUNCTION)
      (and (= (length r2) (length r1))
           (= (length opts1) (length opts2))
           (eq (and (null rest1) (null key-flag1))
               (and (null rest2) (null key-flag2)))
           ;; All keywords mentioned in the genericf function
           ;; must be accepted by the method.
           (or (null key-flag1)
               (null key-flag2)
               a-o-k2
               (null (set-difference (all-keywords keywords1)
                                     (all-keywords keywords2))))
           t))))

#+clasp
(defun update-specializer-profile (generic-function)
  (let ((methods (clos:generic-function-methods generic-function)))
    (if methods
        (let* ((first-method-specializers (method-specializers (car methods)))
               (vec (make-array (length first-method-specializers) :initial-element nil))
               (tc (find-class 't)))
          (loop for method in methods
             for specs = (method-specializers method)
             do (loop for i from 0 below (length vec)
                   for spec in specs
                   for specialized = (not (eq spec tc))
                   when specialized
                   do (setf (elt vec i) t)))
          (setf (generic-function-specializer-profile generic-function) vec))
        (setf (generic-function-specializer-profile generic-function) nil))))

(defun add-method (gf method)
  ;; during boot it's a structure accessor
  (declare (notinline method-qualifiers remove-method))
  ;;
  ;; 1) The method must not be already installed in another generic function.
  ;;
  (let ((other-gf (method-generic-function method)))
    (unless (or (null other-gf) (eq other-gf gf))
      (error "The method ~A belongs to the generic function ~A ~
and cannot be added to ~A." method other-gf gf)))
  ;;
  ;; 2) The method and the generic function should have congruent lambda
  ;;    lists. That is, it should accept the same number of required and
  ;;    optional arguments, and only accept keyword arguments when the generic
  ;;    function does.
  ;;
  (let ((new-lambda-list (method-lambda-list method)))
    (if (slot-boundp gf 'lambda-list)
	(let ((old-lambda-list (generic-function-lambda-list gf)))
	  (unless (congruent-lambda-p old-lambda-list new-lambda-list)
	    (error "Cannot add the method ~A to the generic function ~A because their lambda lists ~A and ~A are not congruent."
		   method gf old-lambda-list new-lambda-list)))
	(reinitialize-instance gf :lambda-list new-lambda-list)))
  ;;
  ;; 3) Finally, it is inserted in the list of methods, and the method is
  ;;    marked as belonging to a generic function.
  ;;
  (when (generic-function-methods gf)
    (let* ((method-qualifiers (method-qualifiers method)) 
	   (specializers (method-specializers method))
	   (found (find-method gf method-qualifiers specializers nil)))
      (when found
	(remove-method gf found))))
  ;;
  ;; We install the method by:
  ;;  i) Adding it to the list of methods
  (push method (generic-function-methods gf))
  (setf (method-generic-function method) gf)
  ;;  ii) Updating the specializers list of the generic function. Notice that
  ;;  we should call add-direct-method for each specializer but specializer
  ;;  objects are not yet implemented
  #+(or)
  (dolist (spec (method-specializers method))
    (add-direct-method spec method))
  ;;  iii) Computing a new discriminating function... Well, since the core
  ;;  ECL does not need the discriminating function because we always use
  ;;  the same one, we just update the spec-how list of the generic function.
  (compute-g-f-spec-list gf)
  (set-generic-function-dispatch gf)
  ;;  iv) Update dependents.
  (update-dependents gf (list 'add-method method))
  ;;  Clasp keeps track of which specializers can be ignored
  (update-specializer-profile gf)
  ;;  v) Register with specializers
  (register-method-with-specializers method)
  gf)

(defun function-to-method (name specializers lambda-list)
  (let ((function (fdefinition name)))
    (install-method 'function-to-method-temp
                    nil
                    specializers
                    lambda-list
                    (lambda (.method-args. .next-methods. #+ecl &rest #+clasp &va-rest args)
                      (apply function args)))
    (setf (fdefinition name) #'function-to-method-temp
          (generic-function-name #'function-to-method-temp) name)
    (fmakunbound 'function-to-method-temp)))

(defun remove-method (gf method)
  (setf (generic-function-methods gf)
	(delete method (generic-function-methods gf))
	(method-generic-function method) nil)
  (si:clear-gfun-hash gf)
  (loop for spec in (method-specializers method)
     do (remove-direct-method spec method))
  (compute-g-f-spec-list gf)
  (set-generic-function-dispatch gf)
  (update-dependents gf (list 'remove-method method))
  ;;  Clasp keeps track of which specializers can be ignored
  (update-specializer-profile gf)
  gf)


;;(setq cmp:*debug-compiler* t)
(function-to-method 'add-method '(standard-generic-function standard-method)
                    '(gf method))
(function-to-method 'remove-method '(standard-generic-function standard-method)
                    '(gf method))

(function-to-method 'find-method '(standard-generic-function t t)
                    '(gf qualifiers specializers &optional error))

;;; COMPUTE-APPLICABLE-METHODS is used by the core in various places,
;;; including instance initialization. This means we cannot just redefine it.
;;; Instead, we create an auxiliary function and move definitions from one to
;;; the other.

#+(or)
(defgeneric aux-compute-applicable-methods (gf args)
  (:method ((gf standard-generic-function) args)
    (std-compute-applicable-methods gf args)))

(defmethod aux-compute-applicable-methods ((gf standard-generic-function) args)
  (std-compute-applicable-methods gf args))
(let ((aux #'aux-compute-applicable-methods))
  (setf (generic-function-name aux) 'compute-applicable-methods
	(fdefinition 'compute-applicable-methods) aux))

#+(or)
(eval-when (:compile-toplevel :load-toplevel)
  ;;#+(or)
  (defgeneric aux-compute-applicable-methods (gf args)
    (:method ((gf standard-generic-function) args)
      (std-compute-applicable-methods gf args)))


  (defmethod aux-compute-applicable-methods ((gf standard-generic-function) args)
    (std-compute-applicable-methods gf args))


  (let ((aux #'aux-compute-applicable-methods))
    (setf (generic-function-name aux) 'compute-applicable-methods
	  (fdefinition 'compute-applicable-methods) aux))
)  

(defmethod compute-applicable-methods-using-classes
    ((gf standard-generic-function) classes)
  (std-compute-applicable-methods-using-classes gf classes))

(function-to-method 'compute-effective-method
                    '(standard-generic-function t t)
                    '(gf method-combination applicable-methods))

;;; ----------------------------------------------------------------------
;;; Error messages

(defmethod no-applicable-method (gf &rest args)
  (error "No applicable method for ~S with ~
          ~:[no arguments~;arguments of types ~:*~{~& ~A~}~]."
         (generic-function-name gf)
         (mapcar #'type-of args)))

(defmethod no-next-method (gf method &rest args)
  (declare (ignore gf))
  (error "In method ~A~%No next method given arguments ~A" method args))

(defun no-primary-method (gf &rest args)
  (error "Generic function: ~A. No primary method given arguments: ~S"
	 (generic-function-name gf) (core:maybe-expand-generic-function-arguments args)))

;;; Now we protect classes from redefinition:
(eval-when (compile load)
(defun setf-find-class (new-value name &optional errorp env)
  (declare (ignore errorp))
  (let ((old-class (find-class name nil env)))
    (cond
      ((typep old-class 'built-in-class)
       (error "The class associated to the CL specifier ~S cannot be changed."
	      name))
      ((member name '(CLASS BUILT-IN-CLASS) :test #'eq)
       (error "The kernel CLOS class ~S cannot be changed." name))
      ((classp new-value)
       #+clasp(core:set-class new-value name)
       #+ecl(setf (gethash name si:*class-name-hash-table*) new-value))
      ((null new-value)
       #+clasp(core:set-class nil name) ; removes class
       #+ecl(remhash name si:*class-name-hash-table*))
      (t (error "~A is not a class." new-value))))
  new-value)
)

;;; ----------------------------------------------------------------------
;;; DEPENDENT MAINTENANCE PROTOCOL
;;;

(defmethod add-dependent ((c class) dep)
  (pushnew dep (class-dependents c)))

(defmethod add-dependent ((c generic-function) dependent)
  (pushnew dependent (generic-function-dependents c)))

(defmethod remove-dependent ((c class) dep)
  (setf (class-dependents c)
        (remove dep (class-dependents c))))

(defmethod remove-dependent ((c standard-generic-function) dep)
  (setf (generic-function-dependents c)
        (remove dep (generic-function-dependents c))))

(defmethod map-dependents ((c class) function)
  (dolist (d (class-dependents c))
    (funcall function d)))

(defmethod map-dependents ((c standard-generic-function) function)
  (dolist (d (generic-function-dependents c))
    (funcall function d)))

(defgeneric update-dependent (object dependent &rest initargs))

;; After this, update-dependents will work
(setf *clos-booted* 'map-dependents)


(defclass initargs-updater ()
  ())

(defun recursively-update-classes (a-class)
  (slot-makunbound a-class 'valid-initargs)
  (mapc #'recursively-update-classes (class-direct-subclasses a-class)))

(defmethod update-dependent ((object generic-function) (dep initargs-updater)
			     &rest initargs)
  (declare (ignore dep initargs object))
  (recursively-update-classes +the-class+))

(let ((x (make-instance 'initargs-updater)))
  (add-dependent #'shared-initialize x)
  (add-dependent #'initialize-instance x)
  (add-dependent #'allocate-instance x))


(function-to-method 'make-method-lambda
                    '(standard-generic-function standard-method t t)
                    '(gf method lambda-form environment))

(function-to-method 'compute-discriminating-function
                    '(standard-generic-function) '(gf))

(function-to-method 'generic-function-method-class
                    '(standard-generic-function) '(gf))

(function-to-method 'find-method-combination
                    '(standard-generic-function t t)
                    '(gf method-combination-type-name method-combination-options))


  
#+clasp
(defun all-generic-functions ()
  (remove-duplicates
   (mapappend #'specializer-direct-generic-functions
              (subclasses* (find-class 't)))))


(defun switch-clos-to-fastgf ()
  (let ((dispatchers (make-hash-table))
        (count 0))
    (dolist (x (clos::all-generic-functions))
      (clos::update-specializer-profile x)
      (if (member x (list
                     #'clos::compute-applicable-methods-using-classes
                     #'clos::add-direct-method
                     #'clos::compute-effective-method
                     #'print-object
                     #'clos::generic-function-name
                     ))
          nil
          (setf (gethash x dispatchers) (clos::calculate-fastgf-dispatch-function x))))
    (maphash (lambda (gf disp)
               (clos::safe-set-funcallable-instance-function gf disp)
               (incf count))
             dispatchers)
    count))

;;; April 2017  Turn this off for now to debug
(eval-when (:execute :compile-toplevel :load-toplevel)
  ;; This turns on fast-dispatch that uses the code in cmpgf.lsp
  ;;   Once clos:*enable-fastgf* is set to T
  ;;     EVERY new generic function starts using
  ;;     the compiled fast dispatch functions
  (when (member :enable-fastgf *features*)
    (format t "!!!!!  enable-fastgf is on - loading sys:kernel;clos;fastgf.lsp~%")
    (load "sys:kernel;clos;fastgf.lsp")
    (format t "!!!!! Turning on *enable-fastgf*~%")
    (setf clos:*enable-fastgf* t)
;;;    (format t "    Switching CLOS functions to fastgf~%")
    #+(or)(let ((count (switch-clos-to-fastgf)))
            (format t "        switched ~a functions~%" count))))

;;; April 2017  Turn this on to at least use fastgf for
;;;    generic functions outside of the core
#+(or)
(eval-when (:execute :compile-toplevel :load-toplevel)
  ;; This turns on fast-dispatch that uses the code in cmpgf.lsp
  ;;   Once clos:*enable-fastgf* is set to T
  ;;     EVERY new generic function starts using
  ;;     the compiled fast dispatch functions
  (setf clos:*enable-fastgf* t))


#|
;;; THIS IS SUPPOSED TO BE AN ACCESSOR of the CLOS:SPECIALIZER class
#+clasp
(defun specializer-direct-generic-functions (class)
  (remove-duplicates
   (mapcar #'method-generic-function
           (specializer-direct-methods class))))
|#
          

