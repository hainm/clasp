(print "Testing")
(progn
  (require :clang-tool)
  (require :clasp-analyzer)
  (print "Done"))

(in-package :clasp-analyzer)

(defparameter *db*
  (clasp-analyzer:setup-clasp-analyzer-compilation-tool-database
   #P"app-resources:build-databases;clasp_compile_commands.json"
   :selection-pattern ".*clangTool.*\.cc.*"
))

(clasp-analyzer:search/generate-code *db*)


(defparameter *db*
  (clasp-analyzer:setup-clasp-analyzer-compilation-tool-database
   #P"app-resources:build-databases;clasp_compile_commands.json"
))
(analyze-only *db*)
(search "bc" "abcdefg")

(maphash (lambda (k v) (when (search "Wrap" k) (print k))) (project-lispallocs *project*))
*project*

(gethash "gctools::GCVector_moveable<gctools::smart_ptr<core::SourceFileInfo_O>>" *classes*)




(progn
  (defparameter *classes* (project-classes *project*))
  (defparameter *analysis* (analyze-project *project*))
  (setq *print-pretty* nil)
  (defparameter *cws* (gethash "core::ClosureWithSlots" *classes*))
  (defparameter *cwr* (gethash "core::ClosureWithRecords" *classes*))
  (defparameter *cc* (gethash "gctools::GCVector_moveable<gctools::smart_ptr<core::SourceFileInfo_O>>" *classes*))
  )
*cwr*


(trace linearize-code-for-field linearize-class-layout-impl)
(untrace)
(defparameter *ls* (class-layout *cws* *analysis*))
*ls*
(defparameter *lr* (class-layout *cwr* *analysis*))

(defparameter *lc* (class-layout *cc* *analysis*))
*lc*
*lr*
*node*
(codegen-layout t "core__ClosureWithRecords" "core::ClosureWithRecords" *lr*)
(codegen-layout t "core__ClosureWithSlots" "core::ClosureWithSlots" *ls*)
(codegen-container-layout t "foo" "bar" *lc*)
(variable-part *lr*)


(fifth *l*)
(codegen-offsets (fifth *l*))

(trace fixable-instance-variables-impl)
(fixable-instance-variables *cws* *analysis*)

(cclass-fields *cws*)
(first (cclass-fields *cws*))
*cws*
(fixable-instance-variables *cws* *analysis*)
(fixable-instance-variables (instance-variable-ctype (car (third (fixable-instance-variables *cws* *analysis*)))) *analysis*)
(car (third (fixable-instance-variables *cws* *analysis*)))
(class-of (first (cclass-fields *cws*)))
(contains-fixptr-p (first (cclass-fields *cws*)) *project*)
(gethash "core::SlotData" *classes*)
(contains-fixptr-p (gethash "core::SlotData" *classes*) *project*)
(gethash "gctools::GCArray_moveable<core::SlotData,0>" *classes*)

(trace contains-fixptr-impl-p)


(code-for-class-layout (cclass-key *cws*) (class-layout *cws* *analysis*) *project*)
(variable-part (class-layout *cws* *analysis*))
(fixable-instance-variables (car (variable-part (class-layout *cws* *analysis*))) *analysis*)
(defparameter *v* (instance-variable-ctype (car (variable-part (class-layout *cws* *analysis*)))))
(fixable-instance-variables *v* *analysis*)

((class-layout *cws* *analysis*)
(defparameter *analysis* (analyze-project *project*))
(fix-code *cws* *analysis*)
*cws*

(defparameter *cons* (gethash "core::Cons_O" *classes*))
(code-for-class-layout (cclass-key *cons*) (class-layout *cons* *analysis*))