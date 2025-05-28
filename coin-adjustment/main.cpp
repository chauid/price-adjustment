#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <windows.h> // sed -i 's/#include <windows.h>/#include <unistd.h>/g' {pwd}/coindb.c
#include <mysql.h> // sed -i '/s/#include <mysql.h>/#include \"/usr/include/mysql/mysql.h\"/g' {pwd}/coindb.c

#define VERSION "0.8" // ���� ���� v0.8

#define READ_MAXCOUNT 50 // �� �� �ִ� �Է� ��

#define CoinNameBufferSize 30 // ���� �̸� ����: 30byte
#define CoinNewsBufferSize 255 // ���� ���� ����: 255byte
#define QueryBufferSize 512 // ���� ����
#define TickInterval 2000 // ������Ʈ ���ð�(ms)

/**
 * @brief ���� ���� �Լ�
 * @param MYSQL* Connect ����, char[] ����
 * @return mysql_store_result(Connect)�� ��
 */
MYSQL_RES* ExcuteQuery(MYSQL* connect, const char* query);

/**
 * @brief ���� ī�� �߱� �Լ�
 * @param ����ID, ���� ��� ����, ���� ���� �ð�, ���� �����
 */
void NewsCardIssue(char* CoinName, char* CoinNews, int* NewsEffect);

int main()
{
	// DB Connection Info
	// [0]: HOST, [1]: HOSTNAME, [2]: HOSTPW, [3]: DB, [4]: PORT
	char DBINFO[5][READ_MAXCOUNT] = { 0, };

	FILE* pFile; // db.conf
	fopen_s(&pFile, "db.conf", "r");
	if (pFile == NULL)
	{
		puts("db.conf is not exist!");
		return 0;
	}
	for (int i = 0; i < 5; i++)
	{
		if (feof(pFile))
		{
			puts("Read File Error - db.conf is invalid!");
			fclose(pFile);
			return 0;
		}
		fgets(DBINFO[i], READ_MAXCOUNT, pFile);
		for (int j = 0; j < strlen(DBINFO[i]); j++)
		{
			if (DBINFO[i][j] == '\n' || DBINFO[i][j] == '\r') DBINFO[i][j] = NULL;
		}
	}
	fclose(pFile);

	MYSQL* Connect = mysql_init(NULL);
	MYSQL_RES* Result = NULL;
	MYSQL_ROW Rows;
	char Query[QueryBufferSize] = "";

	time_t now; // ���� �ð�
	struct tm timeInfo; // �ð� ����
	srand((unsigned int)time(NULL));

#pragma region Program Initalize
	printf("CoinDB Version: %s\nmysql client version:%s\n", VERSION, mysql_get_client_info());
	if (!mysql_real_connect(Connect, DBINFO[0], DBINFO[1], DBINFO[2], DBINFO[3], atoi(DBINFO[4]), NULL, 0)) // DB ���� ���� 
	{
		printf("DB Connection Fail\n\n");
		mysql_close(Connect);
		return 0;
	}
	printf("DB Connection Success!\n\n");

	sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins");
	Result = ExcuteQuery(Connect, Query);
	int CoinCount = 0; // ���� ����
	if ((Rows = mysql_fetch_row(Result)) != NULL) CoinCount = atoi(Rows[0]);
	else
	{
		puts("Program Initalize Error - Check your DB!");
		mysql_close(Connect);
		return 0;
	}

	/* DB data => Heap Memory: ������, or * ������ ���� ũ�� */
	bool memory_ok = true; // �޸� �Ҵ� ����

	int* CoinIds; // INT * CoinCount (Immutable: Read Only)
	char** CoinNames; // VARCHAR(30) * CoinCount (Immutable: Read Only)
	int* CoinPrice; // INT * CoinCount
	int* CoinDefaultPrice; // INT * CoinCount (Immutable: Read Only)
	char** CoinNews; // VARCHAR(255) * CoinCount
	int* CoinFluctuationRate; // INT * CoinCount (Immutable: Read Only)
	int* CoinNewsTerm; // INT * CoinCount
	int* CoinNewsEffect; // INT * CoinCount
	int* CoinDelisting; // INT * CoinCount
	int* CoinIsTrade; // INT * CoinCount (Immutable: Read Only)

	/* �ʱ�ȭ ���� */
	int* CoinOpeningPriceMinute; // ���� �ð� - 10��
	int* CoinLowPriceMinute; // ���� ���� - 10��
	int* CoinHighPriceMinute; // ���� �� - 10��
	int* CoinClosingPriceMinute; // ���� ���� - 10��

	int* CoinOpeningPriceHour; // ���� �ð� - 1�ð�
	int* CoinLowPriceHour; // ���� ���� - 1�ð�
	int* CoinHighPriceHour; // ���� �� - 1�ð�
	int* CoinClosingPriceHour; // ���� ���� - 1�ð�

	/* �޸� �Ҵ�: ���� ���� �Ҵ� + ���� �̸� ���� �Ҵ� */
	CoinIds = (int*)calloc(CoinCount, sizeof(int)); // CoinIds
	CoinNames = (char**)malloc(sizeof(char*) * CoinCount); // CoinNames
	for (int record_index = 0; record_index < CoinCount; record_index++) CoinNames[record_index] = (char*)calloc(CoinNameBufferSize, sizeof(char));
	CoinPrice = (int*)calloc(CoinCount, sizeof(int)); // ���� ����
	CoinDefaultPrice = (int*)calloc(CoinCount, sizeof(int)); // ���� �⺻��
	CoinFluctuationRate = (int*)calloc(CoinCount, sizeof(int)); // �⺻ �ü� ������
	CoinNews = (char**)malloc(sizeof(char*) * CoinCount); // ���δ���
	for (int record_index = 0; record_index < CoinCount; record_index++) CoinNews[record_index] = (char*)calloc(CoinNewsBufferSize, sizeof(char));
	CoinNewsTerm = (int*)calloc(CoinCount, sizeof(int)); // ���δ����Ⱓ
	CoinNewsEffect = (int*)calloc(CoinCount, sizeof(int)); // ���δ��� �����
	CoinDelisting = (int*)calloc(CoinCount, sizeof(int)); // ����Ⱓ
	CoinIsTrade = (int*)calloc(CoinCount, sizeof(int)); // ���� �ŷ� ���� ����

	CoinOpeningPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // ���� �ð�
	CoinOpeningPriceHour = (int*)calloc(CoinCount, sizeof(int)); // ���� �ð�
	CoinLowPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // ���� ����
	CoinLowPriceHour = (int*)calloc(CoinCount, sizeof(int)); // ���� ����
	CoinHighPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // ���� ��
	CoinHighPriceHour = (int*)calloc(CoinCount, sizeof(int)); // ���� ��
	CoinClosingPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // ���� ����
	CoinClosingPriceHour = (int*)calloc(CoinCount, sizeof(int)); // ���� ����

	/* �޸� ���� Ȯ�� */
	for (int i = 0; i < CoinCount; i++)
	{
		if (CoinNames[i] == NULL || CoinNews[i] == NULL)
		{
			for (int j = 0; j < i; j++)
			{
				free(CoinNames[i]);
				free(CoinNews[i]);
			}
			memory_ok = false;
		}
	}
	if (CoinIds == NULL || CoinNames == NULL || CoinPrice == NULL || CoinDelisting == NULL || CoinNews == NULL ||
		CoinNewsTerm == NULL || CoinNewsEffect == NULL || CoinDefaultPrice == NULL || CoinIsTrade == NULL ||
		CoinFluctuationRate == NULL || CoinOpeningPriceMinute == NULL || CoinLowPriceMinute == NULL ||
		CoinHighPriceMinute == NULL || CoinClosingPriceMinute == NULL || CoinOpeningPriceHour == NULL ||
		CoinLowPriceHour == NULL || CoinHighPriceHour == NULL || CoinClosingPriceHour == NULL || memory_ok == false)
	{
		free(CoinIds); free(CoinNames); free(CoinPrice); free(CoinDelisting); free(CoinNews);
		free(CoinNewsTerm); free(CoinNewsEffect); free(CoinDefaultPrice); free(CoinIsTrade);
		free(CoinFluctuationRate); free(CoinOpeningPriceMinute); free(CoinLowPriceMinute);
		free(CoinHighPriceMinute); free(CoinClosingPriceMinute); free(CoinOpeningPriceHour);
		free(CoinLowPriceHour); free(CoinHighPriceHour); free(CoinClosingPriceHour);

		puts("Program Initalize Error - Insufficient (Heap) Memory!");
		mysql_close(Connect);
		return 0;
	}
	/* Character Set */
	sprintf_s(Query, QueryBufferSize, "SET names euckr");
	ExcuteQuery(Connect, Query);
#pragma endregion

#pragma region Load Data
	/* ���� ������ �ҷ�����
	* [0]: id,		[1]: coin_name,	[2]: price,			[3]: default_price,	[4]: fluctuation_rate
	* [5]: news,	[6]: news_term,	[7]: news_effect,	[8]: delisting		[9]: is_trade
	*/
	sprintf_s(Query, QueryBufferSize, "SELECT id, coin_name, price, default_price, fluctuation_rate, news, news_term, news_effect, delisting, is_trade FROM coins");
	Result = ExcuteQuery(Connect, Query);
	for (int fetch_index = 0; fetch_index < CoinCount; fetch_index++)
	{
		if ((Rows = mysql_fetch_row(Result)) != NULL)
		{
			CoinIds[fetch_index] = atoi(Rows[0]);
			sprintf_s(CoinNames[fetch_index], CoinNameBufferSize, "%s", Rows[1]);
			CoinPrice[fetch_index] = atoi(Rows[2]);
			CoinDefaultPrice[fetch_index] = atoi(Rows[3]);
			CoinFluctuationRate[fetch_index] = atoi(Rows[4]);
			sprintf_s(CoinNews[fetch_index], CoinNewsBufferSize, "%s", Rows[5]);
			CoinNewsTerm[fetch_index] = atoi(Rows[6]);
			CoinNewsEffect[fetch_index] = atoi(Rows[7]);
			CoinDelisting[fetch_index] = atoi(Rows[8]);
			CoinIsTrade[fetch_index] = atoi(Rows[9]);
		}
		else
		{
			puts("coinCount is not matched fetch_rows");
			puts("Load Data Error - 0");
			mysql_close(Connect);
			return 0;
		}
	}

	/* ���α׷� ���� ���� [�ð�, ����, �ְ�, ����] = ���簡 */
	for (int coin = 0; coin < CoinCount; coin++)
	{
		CoinOpeningPriceMinute[coin] = CoinPrice[coin];
		CoinOpeningPriceHour[coin] = CoinPrice[coin];
		CoinLowPriceMinute[coin] = CoinPrice[coin];
		CoinLowPriceHour[coin] = CoinPrice[coin];
		CoinHighPriceMinute[coin] = CoinPrice[coin];
		CoinHighPriceHour[coin] = CoinPrice[coin];
		CoinClosingPriceMinute[coin] = CoinPrice[coin];
		CoinClosingPriceHour[coin] = CoinPrice[coin];
	}
#pragma endregion

#pragma region Update Data
	bool IsBackupMinute = false; // 10�и��� backup - ���� �Ⱓ: 1����
	bool IsBackupHour = false; // 1�ð����� backup - ���� �Ⱓ: 2��
	while (true)
	{
		now = time(NULL);
		localtime_s(&timeInfo, &now); // Linux: sed -i 's/localtime_s/localtime_r/g' {pwd}/coindb.c
		if (timeInfo.tm_min % 5 == 0 && timeInfo.tm_min % 10 != 0) IsBackupMinute = false; // %5�и��� backup = false
		if (timeInfo.tm_min % 10 == 0 && !IsBackupMinute) // 10�и��� backup = true
		{
			/* 10�� - {�ð�, ����, ����, ��} ���� �� ���� */
			for (int coin = 0; coin < CoinCount; coin++)
			{
				sprintf_s(Query, QueryBufferSize, "INSERT INTO coins_history_minutely (coin_id, opening_price, closing_price, low_price, high_price, delisted) VALUES (%d, %d, %d, %d, %d, %d)", CoinIds[coin], CoinOpeningPriceMinute[coin], CoinClosingPriceMinute[coin], CoinLowPriceMinute[coin], CoinHighPriceMinute[coin], CoinDelisting[coin]);
				ExcuteQuery(Connect, Query);
				if (Result == NULL) { puts("Update Data Error - 0"); mysql_close(Connect); return 0; }
				/* ���簡 = ���� �ð� = ���� ���� = ���� ���� = ���� �� �ʱ�ȭ */
				CoinOpeningPriceMinute[coin] = CoinPrice[coin];
				CoinClosingPriceMinute[coin] = CoinPrice[coin];
				CoinLowPriceMinute[coin] = CoinPrice[coin];
				CoinHighPriceMinute[coin] = CoinPrice[coin];
			}

			/* 10�� - 1����ġ ������ ���� �� ���� */
			sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins_history_minutely");
			Result = ExcuteQuery(Connect, Query);
			if (Result == NULL) { puts("Update Data Error - 1"); mysql_close(Connect); return 0; }
			if (Rows = mysql_fetch_row(Result))
			{
				if (atoi(Rows[0]) > CoinCount * 6 * 24 * 2)
				{
					sprintf_s(Query, QueryBufferSize, "DELETE FROM coins_history_minutely WHERE id NOT IN (SELECT * FROM (SELECT id FROM coins_history_minutely ORDER BY id DESC LIMIT %d) AS deltable)", CoinCount * 6 * 24 * 2);
					printf("���� ����: %s\n�ð�: %d:%d\n", Query, timeInfo.tm_hour, timeInfo.tm_min);
				}
			}
			IsBackupMinute = true;
		}

		if (timeInfo.tm_min > 0 && timeInfo.tm_min < 60) IsBackupHour = false; // 1~59��: backup = false
		if (timeInfo.tm_min == 0 && !IsBackupHour) // 1�ð����� backup = true
		{
			/* 1�ð� - {�ð�, ����, ����, ��} ���� �� ���� */
			for (int coin = 0; coin < CoinCount; coin++)
			{
				sprintf_s(Query, QueryBufferSize, "INSERT INTO coins_history_hourly (coin_id, opening_price, closing_price, low_price, high_price, delisted) VALUES (%d, %d, %d, %d, %d, %d)", CoinIds[coin], CoinOpeningPriceHour[coin], CoinClosingPriceHour[coin], CoinLowPriceHour[coin], CoinHighPriceHour[coin], CoinDelisting[coin]);
				ExcuteQuery(Connect, Query);
				if (Result == NULL) { puts("Update Data Error - 2"); mysql_close(Connect); return 0; }
				/* ���簡 = ���� �ð� = ���� ���� = ���� ���� = ���� �� �ʱ�ȭ */
				CoinOpeningPriceHour[coin] = CoinPrice[coin];
				CoinClosingPriceHour[coin] = CoinPrice[coin];
				CoinLowPriceHour[coin] = CoinPrice[coin];
				CoinHighPriceHour[coin] = CoinPrice[coin];
			}

			/* 1�ð� - 2��ġ ������ ���� �� ���� */
			sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins_history_hourly");
			Result = ExcuteQuery(Connect, Query);
			if (Result == NULL) { puts("Update Data Error - 3"); mysql_close(Connect); return 0; }
			if (Rows = mysql_fetch_row(Result))
			{
				if (atoi(Rows[0]) > CoinCount * 24 * 365 * 2)
				{
					sprintf_s(Query, QueryBufferSize, "DELETE FROM coins_history_hourly WHERE id NOT IN (SELECT * FROM (SELECT id FROM coins_history_hourly ORDER BY id DESC LIMIT %d) AS deltable)", CoinCount * 24 * 365 * 2);
					printf("���� ����: %s\n�ð�: %d:%d\n", Query, timeInfo.tm_hour, timeInfo.tm_min);
				}
			}
			IsBackupHour = true;
		}

		/* ���� ���� �� ����, ���� ���� */
		for (int coin = 0; coin < CoinCount; coin++) ///////////////////////////////// ���� �˰��� ���� �ʿ�
		{
			// �ŷ� �Ұ� ����
			if (CoinIsTrade[coin] == 0) continue;

			int resultFluctuation = 0; // ���� �ݿ� ������
			int priceFlucDiddle = rand() % 4 + 1; // �⺻ ������ ���� Ȯ��
			int priceDiddle = rand() % 100 + 1; // �ð� ���� Ȯ��
			switch (priceFlucDiddle) // 25% Ȯ��
			{
			case 1:
				resultFluctuation = CoinFluctuationRate[coin] / 4; // �⺻ �������� 25%
				break;
			case 2:
				resultFluctuation = CoinFluctuationRate[coin] / 2; // �⺻ �������� 50%
				break;
			case 3:
				resultFluctuation = CoinFluctuationRate[coin] * 3 / 4;  // �⺻ �������� 75%
				break;
			default: // �⺻ �������� 100%
				break;
			}
			resultFluctuation += CoinNewsEffect[coin] * (CoinFluctuationRate[coin] / 5); // �߰� ���� ������ = ��������� * (�⺻ ������ * 20%)

			if (priceDiddle > 50) resultFluctuation = resultFluctuation; // ��·�: 35%
			else if (priceDiddle > 50) resultFluctuation = -resultFluctuation; // �϶���: 35%
			else resultFluctuation = 0; // ���� ����: 30%
			CoinPrice[coin] += resultFluctuation;

			if (CoinPrice[coin] < CoinLowPriceMinute[coin]) CoinLowPriceMinute[coin] = CoinPrice[coin]; // ���� - 10��
			if (CoinPrice[coin] > CoinHighPriceMinute[coin]) CoinHighPriceMinute[coin] = CoinPrice[coin]; // �� - 10��
			if (CoinPrice[coin] < CoinLowPriceHour[coin]) CoinLowPriceHour[coin] = CoinPrice[coin]; // ���� - 1�ð�
			if (CoinPrice[coin] > CoinHighPriceHour[coin]) CoinHighPriceHour[coin] = CoinPrice[coin]; // �� - 1�ð�

			// �����: ����Ⱓ == 1
			if (CoinDelisting[coin] == 1)
			{
				CoinPrice[coin] = (CoinDefaultPrice[coin] + CoinPrice[coin]) / 2;
				sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s �����!", CoinNames[coin]);
				CoinNewsTerm[coin] = 3600 / (TickInterval / 1000); // 1�ð� ����� ���� ����
				CoinNewsEffect[coin] = 2; // ������ ��·� ����
				CoinDelisting[coin] = 0;
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET price=%d, news='%s', news_term=%d, news_effect=%d, delisting=%d WHERE id=%d", CoinPrice[coin], CoinNews[coin], CoinNewsTerm[coin], CoinNewsEffect[coin], CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// ��������: ���簡 < default_price(-99%) = default_price / 20 && ����Ⱓ == 0 (������)
			if (CoinPrice[coin] < CoinDefaultPrice[coin] / 100 && CoinDelisting[coin] == 0)
			{
				sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s ��������!", CoinNames[coin]);
				CoinNewsTerm[coin] = 0;
				CoinNewsEffect[coin] = 0;
				CoinDelisting[coin] = 3600 / (TickInterval / 1000) + rand() % (3600 / (TickInterval / 1000)); // 1~2�ð� ��������
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET news='%s', news_term=0, news_effect=0, delisting=%d WHERE id=%d", CoinNews[coin], CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// �������� ��: ����Ⱓ > 1
			if (CoinDelisting[coin] > 1)
			{
				if (CoinDelisting[coin] < 180 / (TickInterval / 1000))
				{
					sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s ����� �ҽ� �����...", CoinNames[coin]);
				}
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET news='%s', delisting=%d WHERE id=%d", CoinNews[coin], --CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// ������: ����Ⱓ == 0
			if (CoinDelisting[coin] == 0)
			{
				if (CoinNewsTerm[coin] < 1)
				{
					CoinNewsTerm[coin] = 1800 / (TickInterval / 1000) + rand() % 7200 / (TickInterval / 1000); // 30��~2�ð� ���� ����
					NewsCardIssue(CoinNames[coin], CoinNews[coin], &CoinNewsEffect[coin]); // ���� �߱�
				}
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET price=%d, news='%s', news_term=%d, news_effect=%d WHERE id=%d", CoinPrice[coin], CoinNews[coin], --CoinNewsTerm[coin], CoinNewsEffect[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
			}
		}
		Sleep(TickInterval);
	}
#pragma endregion

	mysql_close(Connect);
	return 0;
}


MYSQL_RES* ExcuteQuery(MYSQL* connect, const char* query)
{
	int status;
	status = mysql_query(connect, query);

	if (status != 0)
	{
		printf("Error : %s\n", mysql_error(connect));
		return NULL;
	}
	else
	{
		return mysql_store_result(connect);
	}
}

#pragma region NewsCard
void NewsCardIssue(char* CoinName, char* CoinNews, int* NewsEffect)
{
	int randomNews = rand() % 6;
	if (strcmp(CoinName, "��������") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�����~~~~~~~!!!");
			*NewsEffect = 5;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ƾƾ�~~~~~~~~~��!!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�����~~~~~~~!!!");
			*NewsEffect = 5;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ƾƾ�~~~~~~~~~��!!");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ñ ���� �λ� ��ȹ�� ��ǥ!");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�ùε�, ��� �ʹ� ��δ�! �̿� ���ñ ���� ����.");
			*NewsEffect = -2;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�������, ��� ����ȭ ���...");
			*NewsEffect = 1;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����� ���� �ҽ�.");
			*NewsEffect = -1;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "��ī����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���������� ���� ��û ����!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��ī���� ������ ���� ȭ�� �߻�!");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ο� ���� ��Ʈ��ũ ��ȹ�� ��ǥ.");
			*NewsEffect = 2;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "������Ʈ��ũ ���� ���� �ҽ�.");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "������ū") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "������ �÷����� �ֿ� ȭ��� ���� ��ū ����� �����ϴ� �߼�.");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���� �÷���, �Ұ����� ������ �����Ǵ�!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���� �÷���, ���� ��ȭ ������Ʈ ����.");
			*NewsEffect = 2;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�̹� �б�, ������ �÷��� ���䷮ ����.");
			*NewsEffect = -1;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "���۸���") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ο� �ε� ���ӻ� ź��! ���۸��� ���� ����!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���� ���� ���ҿ� ���� �ε� ���ӻ� ������ �δ� ����.");
			*NewsEffect = -1;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��� ���ӻ��� Ȱ���� ������ ����!");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�˴ٿ��� ��Ȱ �ҽĿ� �ε� ���ӻ�� ��Ը� �濵 ����!!");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��Ǯ") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��Ǯ����, �е����� �ŷ� �ӵ��� ��ΰ� ���!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��Ǯ ���� �۱� Ÿ�̹� ����, �̿����� �Ҹ� ���...");
			*NewsEffect = -3;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ü�� ��Ʈ��ũ���� ����Ǯ���Ρ�, ���� ���ɼ� ȭ�� TOP10�� ����.");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��Ǯ����, ���۽��� �϶��� ����.");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��������") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��ȣȭ�� �ŷ� �� ���� ������ ��ȣȭ�� ���������Ρ� ����.");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��ȣȭ�� �ŷ� ����, ȭ�� ���� �϶� ���.");
			*NewsEffect = -1;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�������� ������ ��¼�, ������ȭ��� �� �Ҿ �Ǵ� ����� �����ؾ�.��");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�������� �ر� �ҽ� �����...");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ ��ȣȭ�� �ŷ��� �̿밴 �þ�...");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ, ���� �ڱ� ���� ����ϴ�.�� �����ڵ� ����...");
			*NewsEffect = -3;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���� 4���� ��ȣȭ�� �ŷ��ҿ� FTZ ���.");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ �Ļ� ����! ����ȭ�� ������ ����?");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "����������") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����������, ���ü�� �޽��� �� ���ȼ��� �ſ� �پ...");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����������, ���۽����� ���� �ٿ�! ���� ���� �ľ� ��.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "������������ �������� ��Ȧ ����� �˰��� ����. ���ʿ��� �޽����� �ĺ� �����ϴ�. ������ ���� �� ���Ѻ���...��");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����ڰ� ���� �޽����� �������� �޽����� �ν���, �޽��� ���� ���� ���� �߻�! �̿� ������������ �������� ��Ŷ�� ���� �Ұ����ϴ�.��");
			*NewsEffect = -3;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "����ڽ�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ü�� ���� ����, ����ڽ� ���� ��뷮 ����!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����ڽ� 51%% �������� �������� ���� �߻�! �˰��� �ڽ��� ���� ������ �� �ȸ� �������� �г�.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�������� �̿��� �þ�, ����ڽ� ���� �ŷ��� ���� ����.");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�� ���� �ȿ� �𷡰� ��� ������ ���� �߻�! ���̵�, �� �̻� �𷡼� �� ����� ����... ");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%) 
			break;
		}
	}
	else if (strcmp(CoinName, "�ָ���") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��ƺ") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��ƺ�޷�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��ȯ��ū") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "������ȯ�� �����ס� ���� 30.7KM �޼�, ���̺긮�� ���ֵ� ��� ���!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����, ���� �ܼ��� ������ �� ����ݡ�");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�Ϻ��� �����ϰ� ���谡 �����ϸ� �ܽ��� �ָ��ϴ� ������ȯ�� �����ס�");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����, ������ �ý��� �귣�� ��ġ �϶�!");
			*NewsEffect = -3;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "�ù�����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�ù�����, �ݼ��� �� ����� �ʼ� �������� ����!");
			*NewsEffect = 5;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�Ϸ� �Ӛ���, ���� ���� ������� ������... ���!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�Ϸ� �Ӛ���, ���ݼ� �����ϱ�~�� ����.");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�����̽�����, ���� �߶� �ҽ�.");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "���") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "�Ƹ���") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "������") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "��ȭ��ū") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "�����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�����, ����� �Ǹŷ� ����!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "��������� ���ع��� ����! ������� ����� �ź� ���� ����.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "������ ���ǰ� �ҽ�: �������, �ְ��� �ļ���.��");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�� ���� ���� ������ ������� ���� ����-...");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "���̴�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "�������Ǵ�Ƽ") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "���̺�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "���Խ�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "�̴�����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "����ȭ�� ä�� ��! ���� �����, ä���� ������� ��ȯ");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "ä���� ���� ��ȭ, �׷���ī�� ���� ��ȭ����");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "�̴�����, ������ ����ȭ�� TOP3 ����!");
			*NewsEffect = 1;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "���ü�� ��ȣȭ ����� ����, ä���ϱ� �� �������...");
			*NewsEffect = -4;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "ü�θ���") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "ĥ����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "�ڽ����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "������") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "Ǯ�ο�") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "����") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, " ");
			*NewsEffect = 0;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 ���� ����. 2 / 3 Ȯ���� ����
			*NewsEffect = 0; // ��¼� ���� (��·�:50%)
			break;
		}
	}
}
#pragma endregion
