((c-mode .
  ((eval .
	 (progn
	   (setq-local
	    imenu-generic-expression
	    (append '(("Section" "^\/\/ ===+ \\(.*?\\) ===+" 1))
		    imenu-generic-expression))
	   (when (bound-and-true-p evil-mode)
	     (evil-define-key 'normal c-mode-map (kbd "g s") 'consult-imenu)))))))
