#include <bits/stdc++.h>
#include <vector>
#include "common.h"

using namespace std;

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint32_t *lzw_codes) {
	cout << "Encoding\n";
	unordered_map<string, int> table;
	int chunk_length = end_idx - start_idx;
	for (int i = 0; i <= 255; i++) {
		string ch = "";
		ch += char(i);
		table[ch] = i;
	}

	string p = "", c = "";
	p += chunk[0];
	int code = 256;
	int j = 0;
	// cout << "String\tlzw_codes\tAddition\n";
	for (int i = 0; i < chunk_length; i++) {
		if (i != chunk_length - 1)
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
	return lzw_codes;
}