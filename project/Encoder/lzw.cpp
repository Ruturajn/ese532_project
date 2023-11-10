#include <bits/stdc++.h>
#include <vector>
#include "common.h"

using namespace std;

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
         uint16_t *lzw_codes, uint32_t *code_length) {
	//cout << "Encoding\n";
	unordered_map<string, int> table;
	for (int i = 0; i <= 255; i++) {
		string ch = "";
		ch += char(i);
		table[ch] = i;
	}

	string p = "", c = "";
	p += chunk[start_idx];
	int code = 256;
	int j = 0;
	// cout << "String\tlzw_codes\tAddition\n";
	for (int i = start_idx; i < end_idx; i++) {
		if (i != end_idx - 1)
			c += chunk[i + 1];
		if (table.find(p + c) != table.end()) {
			p = p + c;
		}
		else {
			// cout << p << "\t" << table[p] << "\t\t"
			// 	<< p + c << "\t" << code << endl;
			// lzw_codes.push_back(table[p]);
			lzw_codes[j] = table[p];
			table[p + c] = code;
			code++;
			++j;
			p = c;
		}
		c = "";
	}
	// cout << p << "\t" << table[p] << endl;
	// lzw_codes.push_back(table[p]);
	lzw_codes[j] = table[p];
	*code_length = j + 1;
}