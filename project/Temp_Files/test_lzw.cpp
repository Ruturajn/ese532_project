#include <bits/stdc++.h>
#include <string.h>

using namespace std;

vector<int> encoding(string s1) {
    cout << "Encoding\n";
    unordered_map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[ch] = i;
    }

    string p = "", c = "";
    p += s1[0];
    cout << p << endl;
    int code = 256;
    vector<int> output_code;
    cout << "String\tOutput_Code\tAddition\n";
    for (int i = 0; i < s1.length(); i++) {
        if (i != s1.length() - 1)
            c += s1[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        } else {
            cout << p << "\t" << table[p] << "\t\t" << p + c << "\t" << code
                 << endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    cout << p << "\t" << table[p] << endl;
    output_code.push_back(table[p]);
    return output_code;
}

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
         int *lzw_codes, uint32_t *code_length) {
    // cout << "Encoding\n";
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
    cout << "String\tlzw_codes\tAddition\n";
    for (int i = start_idx; i < end_idx; i++) {
        if (i != end_idx - 1)
            c += chunk[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        } else {
            cout << p << "\t" << table[p] << "\t\t" << p + c << "\t" << code
                 << endl;
            // lzw_codes.push_back(table[p]);
            lzw_codes[j] = table[p];
            table[p + c] = code;
            code++;
            ++j;
            p = c;
        }
        c = "";
    }
    cout << p << "\t" << table[p] << endl;
    // lzw_codes.push_back(table[p]);
    lzw_codes[j] = table[p];
    *code_length = j + 1;
}

int main() {
    // string s = "WYS*WYGWYS*WYSWYSG";
    string s = "S*WYGWYS*WYSWYSG";
    unsigned char s1[] = "WYS*WYGWYS*WYSWYSG";
    vector<int> output_code = encoding(s);
    uint32_t packet_len = 0;
    int lzw_codes[1200];
    lzw(s1, 2, strlen((const char *)s1), &lzw_codes[0], &packet_len);
    if (packet_len != output_code.size()) {
        cout << packet_len << "|" << output_code.size() << endl;
        /* exit(EXIT_FAILURE); */
    }
    for (int i = 0; i < output_code.size(); i++) {
        cout << output_code[i] << " ";
    }
    cout << endl;
    for (int i = 0; i < output_code.size(); i++) {
        cout << lzw_codes[i] << " ";
    }
    cout << endl;
}
