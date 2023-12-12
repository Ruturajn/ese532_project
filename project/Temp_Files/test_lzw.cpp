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
