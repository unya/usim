//
//  DataTypes.h
//  usim
//
//  Created by Greg Gilley on 11/10/12.
//
//

#ifndef usim_DataTypes_h
#define usim_DataTypes_h

#define QDTP_MASK			0x1F000000
#define QDTP_SHIFT          24
#define QPOINTER_MASK		0x00FFFFFF

#define DTP_TRAP            0
#define DTP_NULL            01
#define DTP_FREE            02
#define DTP_SYMBOL          03
#define DTP_SYMBOL_HEADER   04
#define DTP_FIX             05
#define DTP_EXTENDED_NUMBER 06
#define DTP_HEADER          07
#define DTP_GC_FORWARD      010
#define DTP_EXTERNAL_VALUE_CELL_POINTER 011
#define DTP_ONE_Q_FORWARD   012
#define DTP_HEADER_FORWARD  013
#define DTP_BODY_FORWARD    014
#define DTP_LOCATIVE        015
#define DTP_LIST            016
#define DTP_U_ENTRY         017
#define DTP_FEF_POINTER     020
#define DTP_ARRAY_POINTER   021
#define DTP_ARRAY_HEADER    022
#define DTP_STACK_GROUP     023
#define DTP_CLOSURE         024
#define DTP_SMALL_FLONUM    025
#define DTP_SELECT_METHOD   026
#define DTP_INSTANCE        027
#define DTP_INSTANCE_HEADER 030
#define DTP_ENTITY          031
#define DTP_STACK_CLOSURE   032
#define DTP_UNUSED_033      033
#define DTP_UNUSED_034      034
#define DTP_UNUSED_035      035
#define DTP_UNUSED_036      036
#define DTP_TRAP_037        037

// function types
#define FT_DTP_U_ENTRY          0
#define FT_DTP_MESA_FEF         01
#define FT_DTP_FEF_POINTER      02
#define FT_DTP_ARRAY_POINTER    03
#define FT_DTP_LIST             07

// header types
#define DTP_HEADER_TYPE_ERROR   0
#define DTP_HEADER_TYPE_FEF     01
#define DTP_HEADER_TYPE_ARRAY_LEADER 02
#define DTP_HEADER_TYPE_MESA_FEF 03
#define DTP_HEADER_TYPE_FLONUM  04
#define DTP_HEADER_TYPE_COMPLEX 05
#define DTP_HEADER_TYPE_BIGNUM  06
#define DTP_HEADER_TYPE_RATIONAL 07

// cdr codes
#define CDR_NORMAL      0
#define CDR_ERROR       01
#define CDR_NIL         02
#define CDR_NEXT        03

#define IS_QDTP_TYPE(o, type) ((o & QDTP_MASK) == type << QDTP_SHIFT)

#define QDTP_HEADER_SHIFT		19
#define QDTP_HEADER_MASK		(0x1f << QDTP_HEADER_SHIFT)

#define IS_HEADER_TYPE(o, type) ((o & QDTP_HEADER_MASK) == type << QDTP_HEADER_SHIFT)

static inline int fixnump(unsigned int o) { return IS_QDTP_TYPE(o, DTP_FIX); }
static inline int fixnum(unsigned int o) { return (int)((o & QPOINTER_MASK) << 8) >> 8; }
//static inline int characterp(unsigned int o) { return IS_QDTP_TYPE(o, DTP_CHARACTER); }
//static inline int character(unsigned int o) { return o & 0xFF; }
static inline int shortfloatp(unsigned int o) { return IS_QDTP_TYPE(o, DTP_SMALL_FLONUM); }
static inline float shortfloat(unsigned int o) { unsigned int intermediate = (o & QPOINTER_MASK) << 7; return *(float *)&intermediate; }

static inline int consp(unsigned int o) { return IS_QDTP_TYPE(o, DTP_LIST); }

struct symbol
{
	unsigned int name;
	unsigned int value;
	unsigned int function;
	unsigned int plist;
	unsigned int package;
	unsigned int setFunction;
	unsigned int flags;
};

static inline int symbolp(unsigned int o) { return IS_QDTP_TYPE(o, DTP_SYMBOL); }
static inline struct symbol *symbol(unsigned int o) { return (struct symbol *)((o & QPOINTER_MASK) << 2); }


void print_cell(unsigned int cell);


#endif
