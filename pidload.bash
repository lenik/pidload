# bash completion for pidload

_pidload()
{
	local cur prev words cword
	_init_completion || return

	case $prev in
	-d|--dev)
		COMPREPLY=($(compgen -f -- "$cur"))
		return
		;;
	-i|--iface)
		local ifaces
		ifaces=$(ls /sys/class/net 2>/dev/null)
		COMPREPLY=($(compgen -W "all $ifaces" -- "$cur"))
		return
		;;
	-o|--output)
		COMPREPLY=($(compgen -d -- "$cur"))
		return
		;;
	-t|--interval|-w|--window|-a|--addr)
		return
		;;
	esac

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '-d --dev -i --iface -a --addr -c --cpu -m --memory -t --interval -w --window -o --output -v --verbose -q --quiet -h --help --version' -- "$cur"))
		return
	fi

	# PIDs and running command names
	local pids cmds
	pids=$(ls /proc 2>/dev/null | grep -E '^[0-9]+$')
	cmds=$(ps -eo comm= 2>/dev/null | sort -u)
	COMPREPLY=($(compgen -W "$pids $cmds" -- "$cur"))
}

complete -F _pidload pidload
