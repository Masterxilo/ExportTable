(* Mathematica Package *)
(* Created by Mathematica Plugin for IntelliJ IDEA *)

(* :Title: ExportTable *)
(* :Context: ExportTable` *)
(* :Author: Paul *)
(* :Date: 2016-08-04 *)

(* :Package Version: 1.0 *)
(* :Mathematica Version: *)
(* :Copyright: (c) 2016 Paul *)
(* :Keywords: *)
(* :Discussion: *)

BeginPackage["ExportTable`"]

Unprotect["ExportTable`*", "ExportTable`Private`*"]
ClearAll["ExportTable`*", "ExportTable`Private`*"]

Options[ExportTable] = {DigitCount -> 17, Method -> "FixedE", Parallelize -> True};

ExportTable::usage = "ExportTable[filename_String, data_ /; MatrixQ[data, Developer`MachineRealQ]]

Generates output compatible with that of Export[filename, data, \"Table\"],
namely printf-%e/%g-style formatted floating point numbers, tab-separated within rows and
newline separated across rows.

These formats are compatible with most C style software using fscanf(\"%g\") to read text files.

This performs best on 2D PackedArrays of floating point numbers. Any input given must be convertable to this.

Options are:

 DigitCount:
  Integer specifying how many significant digits should be included.
  \"G\" may trim trailing zeroes.

  Default: 17

  Note: It seems that Export with \"Table\" uses 16 digits.

 Method:

  \"G\"      Data is formatted using fprintf(\"%g\\t\"). Implies Parallelize -> \"False\". The output is verx similar to that of Export with \"Table\"

  \"FixedE\" Default. Data is formatted using an equivalent of fprintf(\"%e\\t\") with a fixed amount of mantissa and exponent digits
    and always indicating the sign.
    This format currently does not support Inf or NaN values, or values with an exponent of magnitude larger than about 308-DigitCount.
    The files generated will be significantly larger than with %g, especially when DigitCount is large.
    But

 Parallelize: True (Default) / False. Whether multiple cores should be used to write the file. Has no effect for Method -> \"G\"
"

ExportTableUnload::usage = "Release all resources associated with this library.
Note that this does not change $Packages, so Needs[<package name>] will not
reload this package."

Begin["`Private`"]

Ok = True;

MessageAssert~SetAttributes~HoldAll
MessageAssert[cond_, msg_, params___] := If[!TrueQ@cond, Message[msg, params]; Throw@$Failed];
MessageFail~SetAttributes~HoldAll
MessageFail[ msg_, params___] := MessageAssert[False, msg, params];

$LibraryPath = DeleteDuplicates[$LibraryPath~Append~DirectoryName@$InputFileName];

dll = "ExportTable.dll";

ExportTable::initfail = "Failed to initialize."

Check[
  (* [filename, data, digits] *)
  ExportTableG =
      LibraryFunctionLoad[dll,
        "ExportTableG", {"UTF8String", {Real, 2, "Constant"}, Integer}, "Void"];

  (* [filename, data, digits, usePPL] *)
  ExportTableFixedE =
      LibraryFunctionLoad[dll,
        "ExportTableFixedE", {"UTF8String", {Real, 2, "Constant"}, Integer,
        Integer}, "Void"];

  ,
  Message[ExportTable::initfail]; Ok = False;
]

ExportTable::libfunerr = "Calling the library function `` generated errors: ``."
ExportTable::libfunmsg = "Calling the library function `` generated messages."

ExportTable["G", filename_String, data_, options : OptionsPattern[]] := Module[{err},Check[
  MessageAssert[(err = ExportTableG[filename, data, OptionValue@DigitCount]) === Null, ExportTable::libfunerr, "ExportTableG", err];
  , MessageFail[ExportTable::libfunmsg, "ExportTableG"]
  ]];

ExportTable["FixedE", filename_String, data_, options : OptionsPattern[]] := Module[{err},Check[
  MessageAssert[(err = ExportTableFixedE[filename, data, OptionValue@DigitCount, Boole@OptionValue@Parallelize]) === Null, ExportTable::libfunerr, "ExportTableFixedE", err];
  , MessageFail[ExportTable::libfunmsg, "ExportTableFixedE"]
  ]];

ExportTable::param = "ExportTable called with unrecognied parameters ``, should be [filename_String, data_?MatrixQ, options]."
ExportTable::digitcount = "Option DigitCount should be an Integer between 1 and 17, was ``."
ExportTable::parallelize = "Option Parallelize should be a Boolean, was ``."
ExportTable::nsmet = "Option Method should be \"G\" or \"FixedE\", was ``."

ExportTable[filename_String, data_?MatrixQ, options : OptionsPattern[]] := Catch@Module[{},
  (* Parameter verification *)
  MessageAssert[0<OptionValue@DigitCount<=17, ExportTable::digitcount, Short@OptionValue@DigitCount];
  MessageAssert[BooleanQ@OptionValue@Parallelize, ExportTable::parallelize, Short@OptionValue@Parallelize];
  MessageAssert[{"G","FixedE"}~MemberQ~OptionValue@Method, ExportTable::nsmet, Short@OptionValue@Method];

  (* Execute method *)
  ExportTable[OptionValue@Method, filename, data, options];
];

ExportTable[p___] := Catch@MessageFail[ExportTable::param, Short@List@p];

Protect[ExportTable];

UnprotectClearAll~SetAttributes~HoldAll
UnprotectClearAll[x___] := (Unprotect[x];ClearAll[x];)

ExportTableUnload[] := (
  Quiet@LibraryUnload@dll;
  UnprotectClearAll["ExportTable`*", "ExportTable`Private`*"];
  (* TODO should we even Remove the symbols? *)
)

If[!Ok, ExportTableUnload[]];

End[] (* `Private` *)

EndPackage[]