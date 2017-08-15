#include "macroops.h"

void
disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width)
{
	int op, dest, reg, delta, adr;
	int to;
	unsigned int nlc;

	if (!misc_inst_vector_setup) {
		int i, index;
		for (i = 0; i < 1024; i++) {
			if (misc_inst[i].name == 0)
				break;
			index = misc_inst[i].value;
			misc_inst_vector[index] = i;
		}
		misc_inst_vector_setup = 1;
	}
	op = (inst >> 011) & 017;
	dest = (inst >> 015) & 07;
	reg = (inst >> 6) & 07;
	delta = (inst >> 0) & 077;
	printf("%011o%c %06o %s ", loc, even ? 'e' : 'o', inst, op_names[op]);

	switch (op) {
	case 0: // CALL
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		{
			unsigned int v, tag;

			v = get(fefptr + delta);
			tag = (v >> width) & 037;
			switch (tag) {
			case 3:
				v = get(v);
				showstr(v, 0);
				break;
			case 4:
				showstr(v, 0);
				break;
			case 027:
				break;
			default:
				v = get(v);
				show_fef_func_name(v, width);
			}
		}
		break;
	case 2: // MOVE.
	case 3: // CAR
	case 4: // CDR.
	case 5: // CADR.
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		break;
	case 011: // ND1.
		printf("%s ", nd1_names[dest]);
		break;
	case 012: // ND2.
		printf("%s ", nd2_names[dest]);
		break;
	case 013: // ND3.
		printf("%s ", nd3_names[dest]);
		break;
	case 014: // BRANCH.
		printf("type %s, ", branch_names[dest]);

		to = (inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;

		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (loc * 2 + (even ? 0 : 1)) + to;

		if (to > 0) {
			printf("+%o; %o%c ", to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		} else {
			printf("-%o; %o%c ", -to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	case 015: // MISC.
		adr = inst & 0777;
		if (adr < 1024 && misc_inst_vector[adr]) {
			printf("%s ", misc_inst[misc_inst_vector[adr]].name);
		} else {
			printf("%o ", adr);
		}
		printf("dest %s, ", dest_names[dest]);
		break;
	}
	printf("\n");
}
