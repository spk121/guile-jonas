;;; Guile Emacs Lisp

;;; Copyright (C) 2009, 2010, 2011 Free Software Foundation, Inc.
;;;
;;; This library is free software; you can redistribute it and/or
;;; modify it under the terms of the GNU Lesser General Public
;;; License as published by the Free Software Foundation; either
;;; version 3 of the License, or (at your option) any later version.
;;;
;;; This library is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;; Lesser General Public License for more details.
;;;
;;; You should have received a copy of the GNU Lesser General Public
;;; License along with this library; if not, write to the Free Software
;;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

;;; Code:

(define-module (language elisp runtime)
  #:use-module (ice-9 format)
  #:use-module ((system base compile)
                #:select (compile))
  #:export (nil-value
            t-value
            value-slot-module
            function-slot-module
            elisp-bool
            ensure-dynamic!
            symbol-name
            symbol-value
            set-symbol-value!
            symbol-function
            set-symbol-function!
            symbol-plist
            set-symbol-plist!
            symbol-bound?
            symbol-fbound?
            symbol-default-bound?
            symbol-default-value
            set-symbol-default-value!
            bind-symbol
            makunbound!
            fmakunbound!
            symbol-desc
            proclaim-special!
            special?
            emacs!
            unbound
            lexical-binding?
            set-lexical-binding-mode
            log!
            eval-elisp
            compile-elisp
            local-eval-elisp
            make-lisp-string
            lisp-string?)
  #:export-syntax (defspecial prim))

;;; This module provides runtime support for the Elisp front-end.

;;; Values for t and nil. (FIXME remove this abstraction)

(define nil-value #nil)

(define t-value #t)

(define make-lisp-string identity)
(define lisp-string? string?)

;;; Modules for the binding slots.
;;; Note: Naming those value-slot and/or function-slot clashes with the
;;; submodules of these names!

(define value-slot-module (define-module* '(elisp-symbols) #:pure #t))

(define function-slot-module (define-module* '(elisp-functions) #:pure #t))

(define plist-slot-module (define-module* '(elisp-plists) #:pure #t))

(define nil_ 'nil)
(define t_ 't)

;;; Routines for access to elisp dynamically bound symbols.  This is
;;; used for runtime access using functions like symbol-value or set,
;;; where the symbol accessed might not be known at compile-time.  These
;;; always access the dynamic binding and can not be used for the
;;; lexical!

(define lexical-binding #t)

(define (lexical-binding?)
  lexical-binding)

(define (set-lexical-binding-mode x)
  (set! lexical-binding x))

(define unbound (make-symbol "unbound"))

(define dynamic? vector?)
(define (make-dynamic)
  (vector #f 4 0 0 unbound))
(define (dynamic-ref x)
  (vector-ref x 4))
(define (dynamic-set! x v)
  (vector-set! x 4 v))
(define (dynamic-unset! x)
  (vector-set! x 4 unbound))
(define (dynamic-bound? x)
  (not (eq? (vector-ref x 4) unbound)))
(define (dynamic-bind x v thunk)
  (let ((old (vector-ref x 4)))
   (dynamic-wind
     (lambda () (vector-set! x 4 v))
     thunk
     (lambda () (vector-set! x 4 old)))))

(define (ensure-present! module sym thunk)
  (or (module-local-variable module sym)
      (let ((variable (make-variable (thunk))))
        (module-add! module sym variable)
        variable)))

(define (ensure-desc! module sym)
  (ensure-present! module
                   sym
                   (lambda ()
                     (let ((x (make-dynamic)))
                       (vector-set! x 0 sym)
                       x))))

(define (schemify symbol)
  (case symbol
    ((#nil) nil_)
    ((#t) t_)
    (else symbol)))

(define (symbol-name symbol)
  (symbol->string (schemify symbol)))

(define (symbol-desc symbol)
  (let ((symbol (schemify symbol)))
    (let ((module value-slot-module))
      (variable-ref (ensure-desc! module symbol)))))

(define (ensure-dynamic! sym)
  (vector-set! (symbol-desc sym) 3 1))

(define (symbol-dynamic symbol)
  (ensure-dynamic! symbol)
  (symbol-desc symbol))

(define (symbol-value symbol)
  (dynamic-ref (symbol-desc symbol)))

(define (set-symbol-value! symbol value)
  (dynamic-set! (symbol-desc symbol) value)
  value)

(define (symbol-function symbol)
  (cond
   ((module-variable function-slot-module (schemify symbol))
    => variable-ref)
   (else #nil)))

(define (set-symbol-function! symbol value)
  (set! symbol (schemify symbol))
  (ensure-present! function-slot-module symbol (lambda () #nil))
  (let ((module function-slot-module))
   (module-define! module symbol value)
   (module-export! module (list symbol)))
  value)

(define (symbol-plist symbol)
  (set! symbol (schemify symbol))
  (ensure-present! plist-slot-module symbol (lambda () #nil))
  (let ((module plist-slot-module))
    (module-ref module symbol)))

(define (set-symbol-plist! symbol value)
  (set! symbol (schemify symbol))
  (ensure-present! plist-slot-module symbol (lambda () #nil))
  (let ((module plist-slot-module))
   (module-define! module symbol value)
   (module-export! module (list symbol)))
  value)

(define (symbol-bound? symbol)
  (set! symbol (schemify symbol))
  (and
   (module-bound? value-slot-module symbol)
   (let ((var (module-variable value-slot-module
                               symbol)))
     (and (variable-bound? var)
          (if (dynamic? (variable-ref var))
              (dynamic-bound? (variable-ref var))
              #t)))))

(define symbol-default-bound? symbol-bound?)

(define symbol-default-value symbol-value)

(define set-symbol-default-value! set-symbol-value!)

(define (symbol-fbound? symbol)
  (set! symbol (schemify symbol))
  (and
   (module-bound? function-slot-module symbol)
   (variable-bound?
    (module-variable function-slot-module symbol))
   (variable-ref (module-variable function-slot-module symbol))))

(define (bind-symbol symbol value thunk)
  (dynamic-bind (symbol-desc symbol) value thunk))

(define (makunbound! symbol)
  (if (module-bound? value-slot-module symbol)
      (let ((var (module-variable value-slot-module
                                  symbol)))
        (if (and (variable-bound? var) (dynamic? (variable-ref var)))
            (dynamic-unset! (variable-ref var))
            (variable-unset! var))))
    symbol)

(define (fmakunbound! symbol)
  (if (module-bound? function-slot-module symbol)
      (variable-unset! (module-variable function-slot-module symbol)))
  symbol)

(define (special? sym)
  (eqv? (vector-ref (symbol-desc sym) 3) 1))

(define (proclaim-special! sym)
  (vector-set! (symbol-desc sym) 3 1)
  #nil)

(define (emacs! ref set boundp dref dset dboundp bind)
  (set! symbol-value ref)
  (set! set-symbol-value! set)
  (set! symbol-bound? boundp)
  (set! symbol-default-value dref)
  (set! set-symbol-default-value! dset)
  (set! symbol-default-bound? dboundp)
  (set! bind-symbol bind)
  (set! lexical-binding? (lambda () (symbol-value 'lexical-binding)))
  (set! set-lexical-binding-mode (lambda (x) (set-symbol-value! 'lexical-binding x))))

(define (eval-elisp form)
  (eval (compile form #:from 'elisp #:to 'tree-il) (current-module)))

(define (compile-elisp form)
  (compile (compile form #:from 'elisp #:to 'bytecode)
           #:from 'bytecode #:to 'value))

(set-symbol-value! nil_ #nil)
(set-symbol-value! t_ #t)

(define (make-string s) s)

;;; Define a predefined macro for use in the function-slot module.

(define (make-id template-id . data)
  (let ((append-symbols
         (lambda (symbols)
           (string->symbol
            (apply string-append (map symbol->string symbols))))))
    (datum->syntax template-id
                   (append-symbols
                    (map (lambda (datum)
                           ((if (identifier? datum)
                                syntax->datum
                                identity)
                            datum))
                         data)))))

(define-syntax defspecial
  (lambda (x)
    (syntax-case x ()
      ((_ name args body ...)
       (with-syntax ((scheme-name (make-id #'name 'compile- #'name)))
         #'(begin
             (define scheme-name
               (cons 'special-operator (lambda args body ...)))
             (set-symbol-function! 'name scheme-name)))))))
