define snow-echo-function-name-for-continuation
	set $cc = $arg0
	if $cc->context && $cc->context->function && $cc->context->function->desc
		printf "%s", snow_symbol_to_cstr($cc->context->function->desc->name)
	else
		echo [UNNAMED FUNCTION]
	end
end

define snow-save-registers
	set $cc = $arg0
	if $cc != 0x0
		set $cc->reg.rbx = $rbx
		set $cc->reg.rbp = $rbp
		set $cc->reg.rsp = $rsp
		set $cc->reg.r12 = $r12
		set $cc->reg.r13 = $r13
		set $cc->reg.r14 = $r14
		set $cc->reg.r15 = $r15
		set $cc->reg.rip = $rip
	end
end

define snow-restore-registers
	set $cc = $arg0
	if $cc != 0x0
		set $rbx = $cc->reg.rbx
		set $rbp = $cc->reg.rbp
		set $rsp = $cc->reg.rsp
		set $r12 = $cc->reg.r12
		set $r13 = $cc->reg.r13
		set $r14 = $cc->reg.r14
		set $r15 = $cc->reg.r15
		set $rip = $cc->reg.rip
	end
end

define snow-return-chain
	set $top_cc = (SnContinuation*)snow_get_current_continuation()
	snow-save-registers($top_cc)
	
	set $cc = $top_cc
	
	set $_snow_i = 0
	
	printf "Continuation #%d (0x%llx): ", $_snow_i, $cc
	snow-echo-function-name-for-continuation $cc
	echo \n
	backtrace
	echo \n
	
	set $_snow_i = 1
	
	set $cc = $cc->return_to
	while $cc != 0x0
		printf "Continuation #%d (0x%llx): ", $_snow_i, $cc
		snow-echo-function-name-for-continuation $cc
		echo \n
		snow-restore-registers $cc
		backtrace
		echo \n
		set $cc = $cc->return_to
		set $_snow_i = $_snow_i + 1
	end
	
	snow-restore-registers $top_cc
end

define snow-rc
	snow-return-chain
end

define snow-bt
	snow-return-chain
end

define snow-backtrace
	snow-return-chain
end

define snow-inspect
	printf "%s\n", snow_inspect_value($arg0)
end

define snow-to-string
	printf "%s\n", snow_value_to_cstr($arg0)
end

# You can't introspect variable types without grievous hacks. No comment (except this).
define snow-inspect-symbol
	printf "#%s\n", snow_symbol_to_cstr($arg0)
end
