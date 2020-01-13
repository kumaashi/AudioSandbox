#include <stdio.h>
#include <ctype.h>

/*
 *
 * 0x00         C1 : 0x0C  | C2 : 0x18  | C3 : 0x24  | C4 : 0x30  | C5 : 0x3C  | C6 : 0x48  | C7 : 0x54  | C8 : 0x60
 * 0x01        #C1 : 0x0D  |#C2 : 0x19  |#C3 : 0x25  |#C4 : 0x31  |#C5 : 0x3D  |#C6 : 0x49  |#C7 : 0x55  |#C8 : 0x61
 * 0x02         D1 : 0x0E  | D2 : 0x1A  | D3 : 0x26  | D4 : 0x32  | D5 : 0x3E  | D6 : 0x4A  | D7 : 0x56  | D8 : 0x62
 * 0x03        #D1 : 0x0F  |#D2 : 0x1B  |#D3 : 0x27  |#D4 : 0x33  |#D5 : 0x3F  |#D6 : 0x4B  |#D7 : 0x57  |#D8 : 0x63
 * 0x04         E1 : 0x10  | E2 : 0x1C  | E3 : 0x28  | E4 : 0x34  | E5 : 0x40  | E6 : 0x4C  | E7 : 0x58
 * 0x05         F1 : 0x11  | F2 : 0x1D  | F3 : 0x29  | F4 : 0x35  | F5 : 0x41  | F6 : 0x4D  | F7 : 0x59
 * 0x06        #F1 : 0x12  |#F2 : 0x1E  |#F3 : 0x2A  |#F4 : 0x36  |#F5 : 0x42  |#F6 : 0x4E  |#F7 : 0x5A
 * 0x07         G1 : 0x13  | G2 : 0x1F  | G3 : 0x2B  | G4 : 0x37  | G5 : 0x43  | G6 : 0x4F  | G7 : 0x5B
 * 0x08        #G1 : 0x14  |#G2 : 0x20  |#G3 : 0x2C  |#G4 : 0x38  |#G5 : 0x44  |#G6 : 0x50  |#G7 : 0x5C
 * 0x09         A1 : 0x15  | A2 : 0x21  | A3 : 0x2D  | A4 : 0x39  | A5 : 0x45  | A6 : 0x51  | A7 : 0x5D
 * 0x0A        #A1 : 0x16  |#A2 : 0x22  |#A3 : 0x2E  |#A4 : 0x3A  |#A5 : 0x46  |#A6 : 0x52  |#A7 : 0x5E
 * 0x0B         B1 : 0x17  | B2 : 0x23  | B3 : 0x2F  | B4 : 0x3B  | B5 : 0x47  | B6 : 0x53  | B7 : 0x5F
 *
 * */

char buf[1024];

unsigned char table[] = {
	0x15, 0x17,  0x0C,  0x0E, 0x10, 0x11, 0x13,
};

int main() {
	int oct = 4;
	int row = 0;
	int count = 0;
	while( fgets(buf, 1023, stdin) ) {
		char *start = buf;
		char c = '\n';

		while(*start != '\0') {
			c = toupper(*start);
			if(c == '#') break;

			if(c == '.') {
				printf("0x00, ");
			}
			if(c == '>') oct++;
			if(c == '<') oct--;
			if(c == '^') printf("0x7F, ");
			if(c == '\n') {
				if(count) {
					printf("//[0x%02X]", row);
				}
				printf("\n");
			}

			if(c >= 'A' && c <= 'G') {
				int sharp = 0;
				if(*(start + 1) == '+') {
					sharp++;
				}
				int note = table[c - 'A'] + (12 * oct) + sharp;
				if(note < 0) note = 0;
				printf("0x%02X, ", note);
				count++;
			}
			if(c == 'O') {
				char c = *(start + 1);
				if(isdigit(c)) {
					oct = c - '0';
				}
			}
			start++;
		}

		if(count) row++;
		count = 0;
	}
	return 0;
}

