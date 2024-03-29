-module(break_md5).
-define(PASS_LEN, 6).
-define(UPDATE_BAR_GAP, 1000).
-define(BAR_SIZE, 40).
-define(MAX_PROCS, 5).

-export([create_procedures/7]).

-export([break_md5/5]).
-export([break_md5s/1,
		break_md5/1,
		pass_to_num/1, 
		num_to_pass/1 ]).

-export([progress_loop/5]).

% Base ^ Exp

pow_aux(_Base, Pow, 0) ->
    Pow;
pow_aux(Base, Pow, Exp) when Exp rem 2 == 0 ->
    pow_aux(Base*Base, Pow, Exp div 2);
pow_aux(Base, Pow, Exp) ->
    pow_aux(Base, Base * Pow, Exp - 1).

pow(Base, Exp) -> pow_aux(Base, 1, Exp).

%% Number to password and back conversion

num_to_pass_aux(_N, 0, Pass) -> Pass;
num_to_pass_aux(N, Digit, Pass) ->
    num_to_pass_aux(N div 26, Digit - 1, [$a + N rem 26 | Pass]).

num_to_pass(N) -> 
	num_to_pass_aux(N, ?PASS_LEN, []).

pass_to_num_aux([],Num)-> 
	Num;

pass_to_num_aux([C|T],Num)-> 
	pass_to_num_aux(T,Num*26 + C -$a).

pass_to_num(Pass) -> 
	pass_to_num_aux(Pass,0).

%% Hex string to Number

hex_char_to_int(N) ->
    if (N >= $0) and (N =< $9) -> N - $0;
       (N >= $a) and (N =< $f) -> N - $a + 10;
       (N >= $A) and (N =< $F) -> N - $A + 10;
       true                    -> throw({not_hex, [N]})
    end.

hex_string_to_num_aux([], Num) -> Num;
hex_string_to_num_aux([Hex|T], Num) ->
    hex_string_to_num_aux(T, Num*16 + hex_char_to_int(Hex)).

hex_string_to_num(Hex) -> hex_string_to_num_aux(Hex, 0).

%% Progress bar runs in its own process

progress_loop(N, Bound, T1, Sum, New) ->
    receive
         {stop, Pid} ->
            io:fwrite("\r    									 \r"),
            Pid ! stop;
        {stop_hashes, Target_Hashes, Pid} ->
            Pid ! {stop_hashes, Target_Hashes},
            ok;
        {progress_report, Checked} ->
            N2 = N + Checked,
            Full_N = N2 * ?BAR_SIZE div Bound,
            Full = lists:duplicate(Full_N, $=),
            Empty = lists:duplicate(?BAR_SIZE - Full_N, $-),
			T2 = erlang:monotonic_time(seconds),
			if 
                T1==T2 ->
					Summ=Sum,
                    Neww=New+Checked;
                true ->
                    Summ=New,
					Neww=0
                end,
            io:format("\r[~s~s] ~.2f% (~p)", [Full, Empty, N2/Bound*100, Summ]),
            progress_loop(N2, Bound, T2, Summ, Neww)
    end.

%% break_md5/2 iterates checking the possible passwords
break_md5([], _ , _ , _ ,Pid)->
	Pid!finished,
	ok;%No more hashes

break_md5(Target_Hashes,N,N,_,Pid)->
	Pid!{not_found,Target_Hashes},
	ok; % Checked every possible password

break_md5(Target_Hashes, N, Bound, Progress_Pid,Pid) ->
	receive
        {del, New_Hashes} ->
            break_md5(New_Hashes, N, Bound, Progress_Pid, Pid);
        stop -> ok
    after 0 ->
	    if N rem ?UPDATE_BAR_GAP == 0 ->
	            Progress_Pid ! {progress_report, ?UPDATE_BAR_GAP};
	       true ->
	            ok
	    end,
	    Pass = num_to_pass(N),
	    Hash = crypto:hash(md5, Pass),
	    Num_Hash = binary:decode_unsigned(Hash),
	    case lists:member(Num_Hash,Target_Hashes) of 
	    	true ->
	    		io:format("\e[2K\r~.16B: ~s~n", [Num_Hash, Pass]),
	    		Pid!{find, lists:delete(Num_Hash,Target_Hashes)},
	    		break_md5(lists:delete(Num_Hash,Target_Hashes),N+1,Bound,Progress_Pid,Pid);

	    	false ->
	            break_md5(Target_Hashes, N+1, Bound, Progress_Pid,Pid)
	    end
    end.

create_procedures(0,Num_Hashes,Iter,Bound,Progress_Pid,N,List_Pid) ->
	receive
		{stop_hashes,Target_Hashes} -> {not_found,Target_Hashes};
		stop -> ok;
		{find, Target_Hashes} -> 
			Fun = fun(Pid_aux) -> Pid_aux ! {del,Target_Hashes} end,
			lists:foreach(Fun,List_Pid),
			create_procedures(0,Num_Hashes,Iter,Bound,Progress_Pid,N,List_Pid);
			finished ->
				if N == ?MAX_PROCS ->
					Progress_Pid ! {stop,self()},
					create_procedures(0,Num_Hashes,Iter,Bound,Progress_Pid,N,List_Pid);
				true -> create_procedures(0,Num_Hashes,1,Bound,Progress_Pid,N+1,List_Pid)
			end;
			{not_found,Target_Hashes} ->
				if N == ?MAX_PROCS ->
					Progress_Pid ! {stop_hashes,Target_Hashes,self()},
					create_procedures(0,Num_Hashes,Iter,Bound,Progress_Pid,N,List_Pid);
				true -> create_procedures(0,Num_Hashes,1,Bound,Progress_Pid,N+1,List_Pid)
			end
		end;
create_procedures(N,Num_Hashes,Iter,Bound,Progress_Pid,Z,List_Pid)->
	Ini = Bound div ?MAX_PROCS * (N-1),
    Fin = Bound div ?MAX_PROCS * N,
    Pid = spawn(?MODULE, break_md5,[Num_Hashes, Ini, Fin, Progress_Pid, self()]),
	create_procedures(N-1, Num_Hashes, Iter, Bound, Progress_Pid, Z, [Pid | List_Pid]).

%% Break a hash

break_md5s(Hashes) ->
    List_Pid = [],
    Bound = pow(26, ?PASS_LEN),
    Progress_Pid = spawn(?MODULE, progress_loop, [0, Bound, 0, 0, 0]),
    Num_Hashes = lists:map(fun hex_string_to_num/1, Hashes),
    Res = create_procedures(?MAX_PROCS, Num_Hashes, 0, Bound, Progress_Pid, 1, List_Pid),
    %%Progress_Pid ! stop,
    Res.

%% Break a single hash

break_md5(Hash) -> break_md5s([Hash]).