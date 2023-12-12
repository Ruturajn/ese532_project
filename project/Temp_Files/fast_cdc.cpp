#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <vector>

using namespace std;

#define MAXSIZE 4096
#define AVGSIZE 2048
#define MODULUS_MASK_A (AVGSIZE - 1)
#define MODULUS_MASK_S ((AVGSIZE * 4) - 1)
#define MODULUS_MASK_L ((AVGSIZE / 4) - 1)
#define TARGET 0
#define BITS 12
#define MINSIZE 1024
#define BUFFER_LEN 16384

uint64_t GEAR[256] = {
    1553318008,
    574654857,
    759734804,
    310648967,
    1393527547,
    1195718329,
    694400241,
    1154184075,
    1319583805,
    1298164590,
    122602963,
    989043992,
    1918895050,
    933636724,
    1369634190,
    1963341198,
    1565176104,
    1296753019,
    1105746212,
    1191982839,
    1195494369,
    29065008,
    1635524067,
    722221599,
    1355059059,
    564669751,
    1620421856,
    1100048288,
    1018120624,
    1087284781,
    1723604070,
    1415454125,
    737834957,
    1854265892,
    1605418437,
    1697446953,
    973791659,
    674750707,
    1669838606,
    320299026,
    1130545851,
    1725494449,
    939321396,
    748475270,
    554975894,
    1651665064,
    1695413559,
    671470969,
    992078781,
    1935142196,
    1062778243,
    1901125066,
    1935811166,
    1644847216,
    744420649,
    2068980838,
    1988851904,
    1263854878,
    1979320293,
    111370182,
    817303588,
    478553825,
    694867320,
    685227566,
    345022554,
    2095989693,
    1770739427,
    165413158,
    1322704750,
    46251975,
    710520147,
    700507188,
    2104251000,
    1350123687,
    1593227923,
    1756802846,
    1179873910,
    1629210470,
    358373501,
    807118919,
    751426983,
    172199468,
    174707988,
    1951167187,
    1328704411,
    2129871494,
    1242495143,
    1793093310,
    1721521010,
    306195915,
    1609230749,
    1992815783,
    1790818204,
    234528824,
    551692332,
    1930351755,
    110996527,
    378457918,
    638641695,
    743517326,
    368806918,
    1583529078,
    1767199029,
    182158924,
    1114175764,
    882553770,
    552467890,
    1366456705,
    934589400,
    1574008098,
    1798094820,
    1548210079,
    821697741,
    601807702,
    332526858,
    1693310695,
    136360183,
    1189114632,
    506273277,
    397438002,
    620771032,
    676183860,
    1747529440,
    909035644,
    142389739,
    1991534368,
    272707803,
    1905681287,
    1210958911,
    596176677,
    1380009185,
    1153270606,
    1150188963,
    1067903737,
    1020928348,
    978324723,
    962376754,
    1368724127,
    1133797255,
    1367747748,
    1458212849,
    537933020,
    1295159285,
    2104731913,
    1647629177,
    1691336604,
    922114202,
    170715530,
    1608833393,
    62657989,
    1140989235,
    381784875,
    928003604,
    449509021,
    1057208185,
    1239816707,
    525522922,
    476962140,
    102897870,
    132620570,
    419788154,
    2095057491,
    1240747817,
    1271689397,
    973007445,
    1380110056,
    1021668229,
    12064370,
    1186917580,
    1017163094,
    597085928,
    2018803520,
    1795688603,
    1722115921,
    2015264326,
    506263638,
    1002517905,
    1229603330,
    1376031959,
    763839898,
    1970623926,
    1109937345,
    524780807,
    1976131071,
    905940439,
    1313298413,
    772929676,
    1578848328,
    1108240025,
    577439381,
    1293318580,
    1512203375,
    371003697,
    308046041,
    320070446,
    1252546340,
    568098497,
    1341794814,
    1922466690,
    480833267,
    1060838440,
    969079660,
    1836468543,
    2049091118,
    2023431210,
    383830867,
    2112679659,
    231203270,
    1551220541,
    1377927987,
    275637462,
    2110145570,
    1700335604,
    738389040,
    1688841319,
    1506456297,
    1243730675,
    258043479,
    599084776,
    41093802,
    792486733,
    1897397356,
    28077829,
    1520357900,
    361516586,
    1119263216,
    209458355,
    45979201,
    363681532,
    477245280,
    2107748241,
    601938891,
    244572459,
    1689418013,
    1141711990,
    1485744349,
    1181066840,
    1950794776,
    410494836,
    1445347454,
    2137242950,
    852679640,
    1014566730,
    1999335993,
    1871390758,
    1736439305,
    231222289,
    603972436,
    783045542,
    370384393,
    184356284,
    709706295,
    1453549767,
    591603172,
    768512391,
    854125182,
};

unsigned int chunk_gen(unsigned char *buff, unsigned int buff_size) {
    unsigned int hash = 0;
    unsigned int i = MINSIZE;
    unsigned int barrier = AVGSIZE;

    if (buff_size <= MINSIZE)
        return buff_size;

    if (buff_size >= MAXSIZE)
        buff_size = MAXSIZE;

    else if (buff_size <= AVGSIZE)
        barrier = buff_size;

    while (i < barrier) {
        hash = (hash >> 1) + GEAR[buff[i]];
        if (!(hash & MODULUS_MASK_S))
            return i;
        i++;
    }

    while (i < buff_size) {
        hash = (hash >> 1) + GEAR[buff[i]];
        if (!(hash & MODULUS_MASK_L))
            return i;
        i++;
    }

    return i;
}

void fast_cdc(unsigned char *buff, unsigned int buff_size) {
    unsigned int chunk_idx = 0;
    unsigned int i = 0;
    unsigned char *temp = buff;

    cout << "0" << endl;
    unsigned int temp_size = buff_size;

    while (i < buff_size) {
        chunk_idx = chunk_gen(temp, temp_size);
        i += chunk_idx;
        cout << i << endl;
        temp += i;
        temp_size = buff_size - i;
    }

    // cout << buff_size << endl;
    /* if (prev != buff_size) */
    /*     cout << buff_size << endl; */
}

void test_cdc( const char* file )
{
	FILE* fp = fopen(file,"r" );
	if(fp == NULL ){
		perror("fopen error");
		return;
	}

	fseek(fp, 0, SEEK_END); // seek to end of file
	int file_size = ftell(fp); // get current file pointer
	fseek(fp, 0, SEEK_SET); // seek back to beginning of file

    file_size = BUFFER_LEN <= file_size ? BUFFER_LEN : file_size;

	unsigned char* buff = (unsigned char *)malloc((sizeof(unsigned char) * file_size));
	if(buff == NULL)
	{
		perror("not enough space");
		fclose(fp);
		return;
	}

	int bytes_read = fread(&buff[0],sizeof(unsigned char),file_size,fp);

    if (bytes_read == 0) {
        printf("Error reading file\n");
        exit(EXIT_FAILURE);
    }

	fast_cdc(buff, file_size);

    free(buff);
    return;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
        test_cdc("LittlePrince.txt");
    else
	    test_cdc(argv[1]);
	return 0;
}
