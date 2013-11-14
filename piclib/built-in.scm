; Although looking like a magic, it just works.
(define (car x)
  (car x))

(define (cdr x)
  (cdr x))

(define (zero? n)
  (= n 0))

(define (positive? x)
  (> x 0))

(define (negative? x)
  (< x 0))

(define (odd? n)
  (= 0 (floor-remainder n 2)))

(define (even? n)
  (= 1 (floor-remainder n 2)))

(define (gcd n m)
  (if (negative? n)
      (set! n (- n)))
  (if (negative? m)
      (set! m (- m)))
  (if (> n m)
      ((lambda (tmp)
	 (set! n m)
	 (set! m tmp))
       n))
  (if (zero? n)
      m
      (gcd (floor-remainder m n) n)))

(define (lcm n m)
  (/ (* n m) (gcd n m)))

(define (caar p)
  (car (car p)))

(define (cadr p)
  (car (cdr p)))

(define (cdar p)
  (cdr (car p)))

(define (cddr p)
  (cdr (cdr p)))

(define (list . args)
  args)

(define (list? obj)
  (if (null? obj)
      #t
      (if (pair? obj)
	  (list? (cdr obj))
	  #f)))

(define (make-list k . args)
  (if (null? args)
      (make-list k #f)
      (if (zero? k)
	  '()
	  (cons (car args)
		(make-list (- k 1) (car args))))))

(define (length list)
  (if (null? list)
      0
      (+ 1 (length (cdr list)))))

(define (append xs ys)
  (if (null? xs)
      ys
      (cons (car xs)
	    (append (cdr xs) ys))))

(define (reverse list . args)
  (if (null? args)
      (reverse list '())
      (if (null? list)
	  (car args)
	  (reverse (cdr list)
		   (cons (car list) (car args))))))

(define (list-tail list k)
  (if (zero? k)
      list
      (list-tail (cdr list) (- k 1))))

(define (list-ref list k)
  (car (list-tail list k)))

(define (list-set! list k obj)
  (set-car! (list-tail list k) obj))

(define (memq obj list)
  (if (null? list)
      #f
      (if (eq? obj (car list))
	  list
	  (memq obj (cdr list)))))

(define (memv obj list)
  (if (null? list)
      #f
      (if (eqv? obj (car list))
	  list
	  (memq obj (cdr list)))))

(define (assq obj list)
  (if (null? list)
      #f
      (if (eq? obj (caar list))
	  (car list)
	  (assq obj (cdr list)))))

(define (assv obj list)
  (if (null? list)
      #f
      (if (eqv? obj (caar list))
	  (car list)
	  (assq obj (cdr list)))))

(define (list-copy obj)
  (if (null? obj)
      obj
      (cons (car obj)
	    (list-copy (cdr obj)))))

(define (map f list)
  (if (null? list)
      '()
      (cons (f (car list))
	    (map f (cdr list)))))

(define-macro (let bindings . body)
  (if (symbol? bindings)
      (begin
	(define name bindings)
	(set! bindings (car body))
	(set! body (cdr body))
	;; expanded form should be like below:
	;; `(let ()
	;;    (define ,loop
	;;      (lambda (,@vars)
	;;        ,@body))
	;;    (,loop ,@vals))
	(list 'let '()
	      (list 'define name
		    (cons 'lambda (cons (map car bindings) body)))
	      (cons name (map cadr bindings))))
      (cons (cons 'lambda (cons (map car bindings) body))
	    (map cadr bindings))))

(define-macro (cond . clauses)
  (if (null? clauses)
      #f
      (let ((c (car clauses)))
	(let ((test (car c))
	      (if-true (cons 'begin (cdr c)))
	      (if-false (cons 'cond (cdr clauses))))
	  (list 'if test if-true if-false)))))

(define else #t)

(define-macro (and . exprs)
  (if (null? exprs)
      #t
      (let ((test (car exprs))
	    (if-true (cons 'and (cdr exprs))))
	(list 'if test if-true #f))))

(define-macro (or . exprs)
  (if (null? exprs)
      #f
      (let ((test (car exprs))
	    (if-false (cons 'or (cdr exprs))))
	(list 'let (list (list 'it test))
	      (list 'if 'it 'it if-false)))))

(define-macro (quasiquote x)
  (cond
   ((symbol? x) (list 'quote x))
   ((pair? x)
    (cond
     ((eq? 'unquote (car x)) (cadr x))
     ((and (pair? (car x))
	   (eq? 'unquote-splicing (caar x)))
      (list 'append (cadr (car x)) (list 'quasiquote (cdr x))))
     (#t (list 'cons
	       (list 'quasiquote (car x))
	       (list 'quasiquote (cdr x))))))
   (#t x)))

(define-macro (let* bindings . body)
  (if (null? bindings)
      `(let () ,@body)
      `(let ((,(caar bindings)
	      ,@(cdar bindings)))
	 (let* (,@(cdr bindings))
	   ,@body))))

(define-macro (letrec bindings . body)
  (let ((vars (map (lambda (v) `(,v #f)) (map car bindings)))
	(initials (map (lambda (v) `(set! ,@v)) bindings)))
    `(let (,@vars)
       (begin ,@initials)
       ,@body)))

(define-macro (letrec* . args)
  `(letrec ,@args))

(define-macro (when test . exprs)
  (list 'if test (cons 'begin exprs) #f))

(define-macro (unless test . exprs)
  (list 'if test #f (cons 'begin exprs)))

(define (equal? x y)
  (cond
   ((eqv? x y)
    #t)
   ((and (pair? x) (pair? y))
    (and (equal? (car x) (car y))
	 (equal? (cdr x) (cdr y))))
   (else
    #f)))

(define (member obj list . opts)
  (let ((compare (if (null? opts) equal? (car opts))))
    (if (null? list)
	#f
	(if (compare obj (car list))
	    list
	    (member obj (cdr list) compare)))))

(define (assoc obj list . opts)
  (let ((compare (if (null? opts) equal? (car opts))))
    (if (null? list)
	#f
	(if (compare obj (caar list))
	    (car list)
	    (assoc obj (cdr list) compare)))))

(define (values . args)
  (if (and (pair? args)
	   (null? (cdr args)))
      (car args)
      (cons '*values-tag* args)))

(define (call-with-values producer consumer)
  (let ((res (producer)))
    (if (and (pair? res)
	     (eq? '*values-tag* (car res)))
        (apply consumer (cdr res))
        (consumer res))))

(define-macro (do bindings finish . body)
  `(let loop ,(map (lambda (x)
		     (list (car x) (cadr x)))
		   bindings)
     (if ,(car finish)
	 (begin ,@(cdr finish))
	 (begin ,@body
		(loop ,@(map (lambda (x)
			       (if (null? (cddr x))
				   (car x)
				   (car (cddr x))))
			     bindings))))))

;;; 6.2. Numbers

(define (floor/ n m)
  (values (floor-quotient n m)
	  (floor-remainder n m)))

(define (truncate/ n m)
  (values (truncate-quotient n m)
	  (truncate-remainder n m)))

(define (exact-integer-sqrt k)
  (let ((n (exact (sqrt k))))
    (values n (- k (square n)))))

(define (boolean=? . objs)
  (define (every pred list)
    (if (null? list)
	#t
	(if (pred (car list))
	    (every pred (cdr list))
	    #f)))
  (or (every (lambda (x) (eq? x #t)) objs)
      (every (lambda (x) (eq? x #f)) objs)))


(define (symbol=? . objs)
  (define (every pred list)
    (if (null? list)
	#t
	(if (pred (car list))
	    (every pred (cdr list))
	    #f)))
  (let ((sym (car objs)))
    (if (symbol? sym)
	(every (lambda (x)
		 (and (symbol? x)
		      (eq? x sym)))
	       (cdr objs))
	#f)))
