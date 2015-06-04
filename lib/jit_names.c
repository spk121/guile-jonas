/*
 * Copyright (C) 2014, 2015  Free Software Foundation, Inc.
 *
 * This file is part of GNU lightning.
 *
 * GNU lightning is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU lightning is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Authors:
 *	Paulo Cesar Pereira de Andrade
 */

static char *code_name[] = {
    "data",
    "live",		"align",
    "save",		"load",
    "#name",		"#note",
    "label",
    "prolog",
    "ellipsis",
    "allocai",		"allocar",
    "arg",
    "getarg_c",		"getarg_uc",
    "getarg_s",		"getarg_us",
    "getarg_i",		"getarg_ui",
    "getarg_l",
    "putargr",		"putargi",
    "va_start",
    "va_arg",		"va_arg_d",
    "va_end",
    "addr",		"addi",
    "addcr",		"addci",
    "addxr",		"addxi",
    "subr",		"subi",
    "subcr",		"subci",
    "subxr",		"subxi",
    "rsbi",
    "mulr",		"muli",
    "qmulr",		"qmuli",
    "qmulr_u",		"qmuli_u",
    "divr",		"divi",
    "divr_u",		"divi_u",
    "qdivr",		"qdivi",
    "qdivr_u",		"qdivi_u",
    "remr",		"remi",
    "remr_u",		"remi_u",
    "andr",		"andi",
    "orr",		"ori",
    "xorr",		"xori",
    "lshr",		"lshi",
    "rshr",		"rshi",
    "rshr_u",		"rshi_u",
    "negr",		"comr",
    "ltr",		"lti",
    "ltr_u",		"lti_u",
    "ler",		"lei",
    "ler_u",		"lei_u",
    "eqr",		"eqi",
    "ger",		"gei",
    "ger_u",		"gei_u",
    "gtr",		"gti",
    "gtr_u",		"gti_u",
    "ner",		"nei",
    "movr",		"movi",
    "extr_c",		"extr_uc",
    "extr_s",		"extr_us",
    "extr_i",		"extr_ui",
    "htonr_us",
    "htonr_ui",		"htonr_ul",
    "ldr_c",		"ldi_c",
    "ldr_uc",		"ldi_uc",
    "ldr_s",		"ldi_s",
    "ldr_us",		"ldi_us",
    "ldr_i",		"ldi_i",
    "ldr_ui",		"ldi_ui",
    "ldr_l",		"ldi_l",
    "ldxr_c",		"ldxi_c",
    "ldxr_uc",		"ldxi_uc",
    "ldxr_s",		"ldxi_s",
    "ldxr_us",		"ldxi_us",
    "ldxr_i",		"ldxi_i",
    "ldxr_ui",		"ldxi_ui",
    "ldxr_l",		"ldxi_l",
    "str_c",		"sti_c",
    "str_s",		"sti_s",
    "str_i",		"sti_i",
    "str_l",		"sti_l",
    "stxr_c",		"stxi_c",
    "stxr_s",		"stxi_s",
    "stxr_i",		"stxi_i",
    "stxr_l",		"stxi_l",
    "bltr",		"blti",
    "bltr_u",		"blti_u",
    "bler",		"blei",
    "bler_u",		"blei_u",
    "beqr",		"beqi",
    "bger",		"bgei",
    "bger_u",		"bgei_u",
    "bgtr",		"bgti",
    "bgtr_u",		"bgti_u",
    "bner",		"bnei",
    "bmsr",		"bmsi",
    "bmcr",		"bmci",
    "boaddr",		"boaddi",
    "boaddr_u",		"boaddi_u",
    "bxaddr",		"bxaddi",
    "bxaddr_u",		"bxaddi_u",
    "bosubr",		"bosubi",
    "bosubr_u",		"bosubi_u",
    "bxsubr",		"bxsubi",
    "bxsubr_u",		"bxsubi_u",
    "jmpr",		"jmpi",
    "callr",		"calli",
    "prepare",
    "pushargr",		"pushargi",
    "finishr",		"finishi",
    "ret",
    "retr",		"reti",
    "retval_c",		"retval_uc",
    "retval_s",		"retval_us",
    "retval_i",		"retval_ui",
    "retval_l",
    "epilog",
    "arg_f",		"getarg_f",
    "putargr_f",	"putargi_f",
    "addr_f",		"addi_f",
    "subr_f",		"subi_f",
    "rsbi_f",
    "mulr_f",		"muli_f",
    "divr_f",		"divi_f",
    "negr_f",		"absr_f",
    "sqrtr_f",
    "ltr_f",		"lti_f",
    "ler_f",		"lei_f",
    "eqr_f",		"eqi_f",
    "ger_f",		"gei_f",
    "gtr_f",		"gti_f",
    "ner_f",		"nei_f",
    "unltr_f",		"unlti_f",
    "unler_f",		"unlei_f",
    "uneqr_f",		"uneqi_f",
    "unger_f",		"ungei_f",
    "ungtr_f",		"ungti_f",
    "ltgtr_f",		"ltgti_f",
    "ordr_f",		"ordi_f",
    "unordr_f",		"unordi_f",
    "truncr_f_i",	"truncr_f_l",
    "extr_f",		"extr_d_f",
    "movr_f",		"movi_f",
    "ldr_f",		"ldi_f",
    "ldxr_f",		"ldxi_f",
    "str_f",		"sti_f",
    "stxr_f",		"stxi_f",
    "bltr_f",		"blti_f",
    "bler_f",		"blei_f",
    "beqr_f",		"beqi_f",
    "bger_f",		"bgei_f",
    "bgtr_f",		"bgti_f",
    "bner_f",		"bnei_f",
    "bunltr_f",		"bunlti_f",
    "bunler_f",		"bunlei_f",
    "buneqr_f",		"buneqi_f",
    "bunger_f",		"bungei_f",
    "bungtr_f",		"bungti_f",
    "bltgtr_f",		"bltgti_f",
    "bordr_f",		"bordi_f",
    "bunordr_f",	"bunordi_f",
    "pushargr_f",	"pushargi_f",
    "retr_f",		"reti_f",
    "retval_f",
    "arg_d",		"getarg_d",
    "putargr_d",	"putargi_d",
    "addr_d",		"addi_d",
    "subr_d",		"subi_d",
    "rsbi_d",
    "mulr_d",		"muli_d",
    "divr_d",		"divi_d",
    "negr_d",		"absr_d",
    "sqrtr_d",
    "ltr_d",		"lti_d",
    "ler_d",		"lei_d",
    "eqr_d",		"eqi_d",
    "ger_d",		"gei_d",
    "gtr_d",		"gti_d",
    "ner_d",		"nei_d",
    "unltr_d",		"unlti_d",
    "unler_d",		"unlei_d",
    "uneqr_d",		"uneqi_d",
    "unger_d",		"ungei_d",
    "ungtr_d",		"ungti_d",
    "ltgtr_d",		"ltgti_d",
    "ordr_d",		"ordi_d",
    "unordr_d",		"unordi_d",
    "truncr_d_i",	"truncr_d_l",
    "extr_d",		"extr_f_d",
    "movr_d",		"movi_d",
    "ldr_d",		"ldi_d",
    "ldxr_d",		"ldxi_d",
    "str_d",		"sti_d",
    "stxr_d",		"stxi_d",
    "bltr_d",		"blti_d",
    "bler_d",		"blei_d",
    "beqr_d",		"beqi_d",
    "bger_d",		"bgei_d",
    "bgtr_d",		"bgti_d",
    "bner_d",		"bnei_d",
    "bunltr_d",		"bunlti_d",
    "bunler_d",		"bunlei_d",
    "buneqr_d",		"buneqi_d",
    "bunger_d",		"bungei_d",
    "bungtr_d",		"bungti_d",
    "bltgtr_d",		"bltgti_d",
    "bordr_d",		"bordi_d",
    "bunordr_d",	"bunordi_d",
    "pushargr_d",	"pushargi_d",
    "retr_d",		"reti_d",
    "retval_d",
    "movr_w_f",		"movr_ww_d",
    "movr_w_d",
    "movr_f_w",		"movi_f_w",
    "movr_d_ww",	"movi_d_ww",
    "movr_d_w",		"movi_d_w",
};
