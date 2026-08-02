(comment) @comment
(string) @string
(character) @character
(number) @number
(type) @type
(preproc) @keyword.directive

; A form's head is a call unless a later pattern reclassifies it.
(list . (symbol) @function.call)

(list . (symbol) @keyword
  (#match? @keyword
   "^(fn|decl|set|if|do|while|do-while|for|switch|case|default|return|goto|label|struct|union|enum|typedef|defmacro|launch|fnptr)$"))

(list . (symbol) @function.builtin
  (#match? @function.builtin
   "^(init|aref|deref|sizeof|offsetof|alignof)$"))

(list . (symbol) @operator
  (#match? @operator
   "^([-+*/%,]|[<>!=]=|<<?|>>?|&&?|\\|\\|?|\\^|[!~]|\\?:|(\\+|-|\\*|/|%|&|\\||\\^|<<|>>)=|\\+\\+|--|->|\\.)$"))

((symbol) @keyword
 (#match? @keyword "^(break|continue)$"))

(list . (symbol) @_head . (symbol) @function
  (#match? @_head "^(fn|defmacro)$"))

(list . (symbol) @_head . (symbol) @type
  (#match? @_head "^(struct|union|enum|typedef)$"))

(list . (symbol) @_head . (symbol) @variable
  (#match? @_head "^decl$"))
