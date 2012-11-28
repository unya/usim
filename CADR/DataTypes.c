//
//  DataTypes.c
//  usim
//
//  Created by Greg Gilley on 11/10/12.
//
//

#include <stdio.h>
#include "DataTypes.h"


void print_cell(unsigned int cell)
{
    printf("cell: %08x  ", cell);
    switch ((cell & QDTP_MASK) >> QDTP_SHIFT)
    {
        case DTP_TRAP:
            printf("dtp-trap\n");
            break;
        case DTP_NULL:
            printf("dtp-null\n");
            break;
        case DTP_FREE:
            printf("dtp-free\n");
            break;
        case DTP_SYMBOL:
            printf("dtp-symbol\n");
            break;
        case DTP_SYMBOL_HEADER:
            printf("dtp-symbol-header\n");
            break;
        case DTP_FIX:
            printf("dtp-fix %d\n", fixnum(cell));
            break;
        case DTP_EXTENDED_NUMBER:
            printf("dtp-extended-number\n");
            break;
        case DTP_HEADER:
            printf("dtp-header\n");
            break;
        case DTP_GC_FORWARD:
            printf("dtp-gc-forward\n");
            break;
        case DTP_EXTERNAL_VALUE_CELL_POINTER:
            printf("dtp-external-value-cell-pointer\n");
            break;
        case DTP_ONE_Q_FORWARD:
            printf("dtp-one-q-forward\n");
            break;
        case DTP_HEADER_FORWARD:
            printf("dtp-header-forward\n");
            break;
        case DTP_BODY_FORWARD:
            printf("dtp-body-forward\n");
            break;
        case DTP_LOCATIVE:
            printf("dtp-locative\n");
            break;
        case DTP_LIST:
            printf("dtp-list\n");
            break;
        case DTP_U_ENTRY:
            printf("dtp-u-entry\n");
            break;
        case DTP_FEF_POINTER:
            printf("dtp-fef-pointer\n");
            break;
        case DTP_ARRAY_POINTER:
            printf("dtp-array-pointer\n");
            break;
        case DTP_STACK_GROUP:
            printf("dtp-stack-group\n");
            break;
        case DTP_CLOSURE:
            printf("dtp-closure\n");
            break;
        case DTP_SMALL_FLONUM:
            printf("dtp-small-flonum\n");
            break;
        case DTP_SELECT_METHOD:
            printf("dtp-select-method\n");
            break;
        case DTP_INSTANCE:
            printf("dtp-instance\n");
            break;
        case DTP_INSTANCE_HEADER:
            printf("dtp-instance-header\n");
            break;
        case DTP_ENTITY:
            printf("dtp-entity\n");
            break;
        case DTP_STACK_CLOSURE:
            printf("dtp-stack-closure\n");
            break;
        default:
            printf("unknown 0%o\n", (cell & QDTP_MASK) >> QDTP_SHIFT);
            break;
    }
}
