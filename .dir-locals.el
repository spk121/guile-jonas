;; Per-directory local variables for GNU Emacs 23 and later.

((nil             . ((fill-column . 72)
                     (tab-width   .  8)))
 (c-mode          . ((c-file-style . "gnu")
                     (indent-tabs-mode . nil)))
 (scheme-mode
  . ((indent-tabs-mode . nil)
     (eval . (put 'let/ec              'scheme-indent-function 1))
     (eval . (put 'pass-if             'scheme-indent-function 1))
     (eval . (put 'pass-if-exception   'scheme-indent-function 2))
     (eval . (put 'pass-if-equal       'scheme-indent-function 2))
     (eval . (put 'with-test-prefix    'scheme-indent-function 1))
     (eval . (put 'with-test-prefix/c&e 'scheme-indent-function 1))
     (eval . (put 'with-code-coverage  'scheme-indent-function 1))
     (eval . (put 'with-statprof       'scheme-indent-function 1))
     (eval . (put 'let-gensyms         'scheme-indent-function 1))
     (eval . (put 'let-fresh           'scheme-indent-function 2))
     (eval . (put 'with-fresh-name-state 'scheme-indent-function 1))
     (eval . (put 'with-fresh-name-state-from-dfg 'scheme-indent-function 1))
     (eval . (put 'with-cps            'scheme-indent-function 1))
     (eval . (put 'with-cps-constants  'scheme-indent-function 1))
     (eval . (put 'with-lexicals       'scheme-indent-function 2))
     (eval . (put 'build-cps-term      'scheme-indent-function 0))
     (eval . (put 'build-cps-exp       'scheme-indent-function 0))
     (eval . (put 'build-cps-cont      'scheme-indent-function 0))
     (eval . (put 'rewrite-cps-term    'scheme-indent-function 1))
     (eval . (put 'rewrite-cps-cont    'scheme-indent-function 1))
     (eval . (put 'rewrite-cps-exp     'scheme-indent-function 1))
     (eval . (put 'build-term          'scheme-indent-function 0))
     (eval . (put 'build-exp           'scheme-indent-function 0))
     (eval . (put 'build-cont          'scheme-indent-function 0))
     (eval . (put 'rewrite-term        'scheme-indent-function 1))
     (eval . (put 'rewrite-cont        'scheme-indent-function 1))
     (eval . (put 'rewrite-exp         'scheme-indent-function 1))
     (eval . (put '$letk               'scheme-indent-function 1))
     (eval . (put '$letk*              'scheme-indent-function 1))
     (eval . (put '$letconst           'scheme-indent-function 1))
     (eval . (put '$continue           'scheme-indent-function 2))
     (eval . (put '$branch             'scheme-indent-function 3))
     (eval . (put '$prompt             'scheme-indent-function 3))
     (eval . (put '$kargs              'scheme-indent-function 2))
     (eval . (put '$kfun               'scheme-indent-function 4))
     (eval . (put '$letrec             'scheme-indent-function 3))
     (eval . (put '$kclause            'scheme-indent-function 1))
     (eval . (put '$fun                'scheme-indent-function 1))
     (eval . (put 'syntax-parameterize 'scheme-indent-function 1))))
 (emacs-lisp-mode . ((indent-tabs-mode . nil)))
 (texinfo-mode    . ((indent-tabs-mode . nil)
                     (fill-column . 72))))
