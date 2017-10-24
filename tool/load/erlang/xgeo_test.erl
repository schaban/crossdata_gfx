-module(xgeo_test).
-export([main/0]).

read_pts(<<>>) -> [];
read_pts(Bin) ->
	<<
		X:32/little-float,
		Y:32/little-float,
		Z:32/little-float,
		Next/binary
	>> = Bin,
	[{X, Y, Z} | read_pts(Next)].

read_pol_data(Bin, Offs, Arity, IdxByteSize) ->
	IdxDataSize = Arity*IdxByteSize,
	<<_:Offs/binary, PolBin:IdxDataSize/binary, _/binary>> = Bin,
	read_int_lst(PolBin, IdxByteSize).
read_pol(Lst, _, [], [], _) -> Lst;
read_pol(Lst, Bin, [Offs | OffsT], [Arity | ArityT], IdxByteSize) ->
	[read_pol_data(Bin, Offs, Arity, IdxByteSize) | read_pol(Lst, Bin, OffsT, ArityT, IdxByteSize)].
read_pols(Bin, OffsLst, ArityLst, IdxByteSize) -> read_pol([], Bin, OffsLst, ArityLst, IdxByteSize).

read_int_lst(Bin, BytesPerVal) ->
	BitsPerVal = BytesPerVal*8,
	[Val || <<Val:BitsPerVal/little>> <= Bin].

write_vtx(Out, V) -> io:format(Out, "v ~f ~f ~f~n", [element(1, V), element(2, V), element(3, V)]).

write_pts(_, []) -> [];
write_pts(Out, [H | T]) -> [write_vtx(Out, H) | write_pts(Out, T)].

write_idx(_, []) -> [];
write_idx(Out, [H | T]) -> [io:format(Out, " ~w", [H + 1]) | write_idx(Out, T)].

write_pol(Out, P) ->
	io:format(Out, "f", []),
	write_idx(Out, lists:reverse(P)),
	io:format(Out, "~n", []).

write_pols(_, []) -> [];
write_pols(Out, [H | T]) -> [write_pol(Out, H) | write_pols(Out, T)].

hex(Val) -> integer_to_list(Val, 16).

xgeo_read(Bin) ->
	<<
		Kind:4/binary,
		Flg:32/little,
		FileSize:32/little,
		HeadSize:32/little,
		StrOffs:32/little,
		NameId:16/little-signed-integer,
		PathId:16/little-signed-integer,
		_:64,
		
		MinX:32/little-float,
		MinY:32/little-float,
		MinZ:32/little-float,
		MaxX:32/little-float,
		MaxY:32/little-float,
		MaxZ:32/little-float,

		PntNum:32/little,
		PolNum:32/little,
		MtlNum:32/little,
		GlbAttrNum:32/little,
		PntAttrNum:32/little,
		PolAttrNum:32/little,
		PntGrpNum:32/little,
		PolGrpNum:32/little,
		SkinNodesNum:32/little,
		MaxSkinWgt:16/little,
		MaxPolVtx:16/little,
		
		OffsPnt:32/little,
		OffsPol:32/little,
		OffsMtl:32/little,
		OffsGlbAttr:32/little,
		OffsPntAttr:32/little,
		OffsPolAttr:32/little,
		OffsPntGrp:32/little,
		OffsPolGrp:32/little,
		OffsSkinNodes:32/little,
		OffsSkin:32/little,
		OffsBVH:32/little,

		_/binary
	>> = Bin,

	SamePolSize = Flg band 1 =/= 0,
	SamePolMtl = Flg band 2 =/= 0,
	HasSkinSpheres = Flg band 4 =/= 0,
	PlanarCvxQuads = Flg band 8 =/= 0,
	IdxByteSize = if PntNum =< 1 bsl 8 -> 1; true -> if PntNum =< 1 bsl 16 -> 2; true -> 3 end end,
	MtlIdSize = if SamePolMtl -> 0; true -> if MtlNum < 1 bsl 7 -> 1; true -> 2 end end,
	PolAritySize = if MaxPolVtx < 1 bsl 8 -> 1; true -> 2 end,
	PntDataSize = PntNum * 12,

	io:format("Kind: ~s~n", [Kind]),
	io:format("Flags: 0x~s~n", [hex(Flg)]),
	io:format("  SamePolSize = ~w~n", [SamePolSize]),
	io:format("  SamePolMtl = ~w~n", [SamePolMtl]),
	io:format("  HasSkinSpheres = ~w~n", [HasSkinSpheres]),
	io:format("  PlanarCvxQuads = ~w~n", [PlanarCvxQuads]),
	io:format("File size: 0x~s~n", [hex(FileSize)]),
	io:format("Header size: 0x~s~n", [hex(HeadSize)]),
	io:format("String List @ 0x~s~n", [hex(StrOffs)]),
	io:format("Name: ~w, Path: ~w~n", [NameId, PathId]), 
	io:format("BBox.min = ~f, ~f, ~f~n", [MinX, MinY, MinZ]),
	io:format("BBox.max = ~f, ~f, ~f~n", [MaxX, MaxY, MaxZ]),
	io:format("~w points @ 0x~s (0x~s bytes)~n", [PntNum, hex(OffsPnt), hex(PntDataSize)]),
	io:format("~w polygons @ 0x~s (bytes per idx = ~w)~n", [PolNum, hex(OffsPol), IdxByteSize]),
	io:format("~w materials @ 0x~s (id size = ~w)~n", [MtlNum, hex(OffsMtl), MtlIdSize]),
	io:format("~w global attrs @ 0x~s~n", [GlbAttrNum, hex(OffsGlbAttr)]),
	io:format("~w point attrs @ 0x~s~n", [PntAttrNum, hex(OffsPntAttr)]),
	io:format("~w polygon attrs @ 0x~s~n", [PolAttrNum, hex(OffsPolAttr)]),
	io:format("~w point groups @ 0x~s~n", [PntGrpNum, hex(OffsPntGrp)]),
	io:format("~w polygon groups @ 0x~s~n", [PolGrpNum, hex(OffsPolGrp)]),
	io:format("~w skin nodes @ 0x~s~n", [SkinNodesNum, hex(OffsSkinNodes)]),
	io:format("skin data @ 0x~s~n", [hex(OffsSkin)]),
	io:format("max skin weights = ~w~n", [MaxSkinWgt]),
	io:format("max vertices per polygon = ~w~n", [MaxPolVtx]),
	io:format("Bounding Volumes Hierarchy @ 0x~s~n", [hex(OffsBVH)]),

	<<
		_:OffsPnt/binary,
		PntData:PntDataSize/binary,
		_/binary
	>> = Bin,
	Pts = read_pts(PntData),

	case SamePolSize of
		true ->
			PolOffsLst = [OffsPol + MtlIdSize*PolNum + X*IdxByteSize*MaxPolVtx || X <- lists:seq(0, PolNum - 1)],
			PolArityLst = lists:duplicate(PolNum, MaxPolVtx);
		false ->
			PolOffsDataSize = PolNum*4,
			<<_:OffsPol/binary, PolOffsBin:PolOffsDataSize/binary, _/binary>> = Bin,
			PolOffsLst = read_int_lst(PolOffsBin, 4),
			PolArityDataSize = PolNum*PolAritySize,
			PolArityTop = OffsPol + PolOffsDataSize + MtlIdSize*PolNum,
			<< _:PolArityTop/binary, PolArityBin:PolArityDataSize/binary, _/binary>> = Bin,
			PolArityLst = read_int_lst(PolArityBin, PolAritySize)
	end,
	Pols = read_pols(Bin, PolOffsLst, PolArityLst, IdxByteSize),

	{ok, Out} = file:open("__erl_xgeo.obj", [write]),
	write_pts(Out, Pts),
	write_pols(Out, Pols),
	file:close(Out),

	io:format("Done!~n", [])
	.

xgeo_load(FPath) ->
	case file:read_file(FPath) of
		{ok, Bin} ->
			io:format("---- ~s -------- ~n", [FPath]),
			xgeo_read(Bin)
			;
		{error, Reason} ->
			{error, file:format_error(Reason)}
	end.

main() ->
	xgeo_load("_test.xgeo").

