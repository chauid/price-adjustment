#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h> // windows 환경 build
//#include <unistd.h> // linuxd 환경 build
#include <mysql.h>
//#include "/usr/include/mysql/mysql.h" // linux 환경 build

constexpr auto VERSION = "0.7"; // 현재 버전 v0.7

constexpr int CoinNameBufferSize = 30; // 코인 이름 길이: 30byte
constexpr int CoinNewsBufferSize = 255; // 코인 뉴스 길이: 255byte


/**
 * @brief 쿼리 실행 함수
 * @param MYSQL* Connect 변수, 쿼리
 * @return mysql_store_result(Connect)의 값
 */
MYSQL_RES* ExcuteQuery(MYSQL* connect, char query[]);

/**
 * @brief coinshistory테이블에 INSERT 수행
 * @param 코인명, 시가, 종가, 저가, 고가, 상폐여부
 */
void BackupData(char names[][30], int opening[], int closing[], int low[], int high[], bool delisted[]);

/**
 * @brief 뉴스 카드 발급 함수
 * @param 코인명, 변경 대상 뉴스, 뉴스 지속 시간, 뉴스 영향력
 */
void NewCardIssue(char* CoinName, char* CoinNews, int* NewsContinuous, int* NewsEffect);

int main()
{
	const char HOST[] = "localhost";
	const char HOSTNAME[] = "root";
	const char HOSTPW[] = "";
	const char DB[] = "coindb";
	const int PORT = 3306;

	MYSQL* Connect = mysql_init(NULL);
	MYSQL_RES* Result = NULL;
	MYSQL_ROW Rows;
	char Query[400] = { 0 }; // 쿼리 최대 길이: 400byte

	printf("CoinDB Version: %s\nmysql client version:%s\n", VERSION, mysql_get_client_info());
	if (mysql_real_connect(Connect, HOST, HOSTNAME, HOSTPW, DB, PORT, NULL, 0)) // DB 연결 성공 
	{
		printf("DB Connection Success!\n\n");

		sprintf_s(Query, sizeof(Query), "SELECT COUNT(*) FROM coins");
		Result = ExcuteQuery(Connect, Query);
		unsigned int CoinCount = 0; // 코인 개수
		if ((Rows = mysql_fetch_row(Result)) != NULL) CoinCount = atoi(Rows[0]);
		else { puts("DB coins테이블의 카디널리티를 가져올 수 없습니다."); mysql_close(Connect); return 0; }

		if (CoinCount > 1000) { puts("비정상적인 카디널리티: coins테이블 또는 쿼리를 확인해주세요."); mysql_close(Connect); return 0; }

		/* DB data => Heap Memory: 카디널리티, or * 데이터 버퍼 크기 */
		bool memory_ok = true; // 메모리 할당 상태

		char** CoinNames; // VARCHAR(30) * CoinCount
		int* CoinPrice; // INT * CoinCount
		bool* CoinDelisted; // TINYINT(1): bool * CoinCount
		int* CoinDelistingTerm; // INT * CoinCount
		char** CoinNews; // TEXT * CoinCount
		int* CoinNewsTerm; // INT * CoinCount
		int* CoinNewsEffect; // INT * CoinCount
		int* CoindefaultPrice; // INT * CoinCount
		int* CoinorderQuantity; // INT * CoinCount

		/* 초기화 변수 */
		int* CoinOpeningPrice; // 코인 시가
		int* CoinLowPrice; // 코인 저가
		int* CoinHighPrice; // 코인 고가
		int* CoinClosingPrice; // 코인 종가

		/* 메모리 할당: 코인 개수 할당 + 코인 이름 길이 할당 */
		CoinNames = (char**)malloc(sizeof(char*) * CoinCount); // CoinNames
		for (int record_index = 0; record_index < CoinCount; record_index++) CoinNames[record_index] = (char*)calloc(CoinNameBufferSize, sizeof(char));
		CoinPrice = (int*)calloc(CoinCount, sizeof(int)); // CoinPrice
		CoinDelisted = (bool*)calloc(CoinCount, sizeof(bool)); // CoinDelisted
		CoinDelistingTerm = (int*)calloc(CoinCount, sizeof(int)); // CoinDelistingTerm
		CoinNews = (char**)malloc(sizeof(char*) * CoinCount); // CoinNews
		for (int record_index = 0; record_index < CoinCount; record_index++) CoinNews[record_index] = (char*)calloc(CoinNewsBufferSize, sizeof(char));
		CoinNewsTerm = (int*)calloc(CoinCount, sizeof(int)); // CoinNewsTerm
		CoinNewsEffect = (int*)calloc(CoinCount, sizeof(int)); // CoinNewsEffect
		CoindefaultPrice = (int*)calloc(CoinCount, sizeof(int)); // CoindefaultPrice
		CoinorderQuantity = (int*)calloc(CoinCount, sizeof(int)); // CoinorderQuantity
		CoinOpeningPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 시가
		CoinLowPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 저가
		CoinHighPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 고가
		CoinClosingPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 종가

		/* 메모리 부족 확인 */
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
		if (CoinNames == NULL || CoinPrice == NULL || CoinDelisted == NULL || CoinDelistingTerm == NULL ||
			CoinNews == NULL || CoinNewsTerm == NULL || CoinNewsEffect == NULL || CoindefaultPrice == NULL ||
			CoinorderQuantity == NULL || CoinOpeningPrice == NULL || CoinLowPrice == NULL || CoinHighPrice == NULL ||
			CoinClosingPrice == NULL || memory_ok == false)
		{
			puts("Insufficient Memory!");
			free(CoinNames); free(CoinPrice); free(CoinDelisted); free(CoinDelistingTerm);
			free(CoinNews); free(CoinNewsTerm); free(CoinNewsEffect); free(CoindefaultPrice);
			free(CoinorderQuantity); free(CoinOpeningPrice); free(CoinLowPrice); free(CoinHighPrice);
			free(CoinClosingPrice);

			mysql_close(Connect);
			return 0;
		}

		bool IsBackup = false; // 10분마다 backup 
		bool IsBackup1 = false; // 1시간마다 backup

		/* Character Set */
		sprintf_s(Query, sizeof(Query), "SET names euckr");
		ExcuteQuery(Connect, Query);

		/* 코인 목록 불러오기 */
		sprintf_s(Query, sizeof(Query), "SELECT coinName, price, delisted, delistingTerm, news, newsTerm, newsEffect, defaultPrice, orderQuantity FROM coins");
		Result = ExcuteQuery(Connect, Query);
		for (int fetch_index = 0; fetch_index < CoinCount; fetch_index++)
		{
			if ((Rows = mysql_fetch_row(Result)) != NULL)
			{
				sprintf_s(CoinNames[fetch_index], CoinNameBufferSize, "%s", Rows[0]);
				CoinPrice[fetch_index] = atoi(Rows[1]);
				CoinDelisted[fetch_index] = atoi(Rows[2]);
				CoinDelistingTerm[fetch_index] = atoi(Rows[3]);
				sprintf_s(CoinNews[fetch_index], CoinNewsBufferSize, "%s", Rows[4]);
				CoinNewsTerm[fetch_index] = atoi(Rows[5]);
				CoinNewsEffect[fetch_index] = atoi(Rows[6]);
				CoindefaultPrice[fetch_index] = atoi(Rows[7]);
				CoinorderQuantity[fetch_index] = atoi(Rows[8]);
			}
			else puts("DB의 코인 개수를 확인해주세요");
		}

		// 일단 coins테이블에서 불러오기까지 함
		// 이제 본격적인 코인 가격 변동 로직 구현해야 함







		// 백업할 때 쓰는 걸로 하자. 현재가는 coins테이블에서 불러오기
		/* 초기 가격 데이터 불러오기: coinshistory테이블의 id 역순 */
		sprintf_s(Query, sizeof(Query), "SELECT closingPrice FROM coinshistory ORDER BY id DESC LIMIT %d", CoinCount);
		Result = ExcuteQuery(Connect, Query);
		for (int fetch_index = CoinCount - 1; fetch_index > 0; fetch_index--)
		{
			if ((Rows = mysql_fetch_row(Result)) != NULL) CoinPrice[fetch_index] = atoi(Rows[0]);
			else // 코인 개수만큼 코인 목록을 불러올 때 해당 값이 없을 경우, 가장 최근 값으로 불러오기 
			{
				MYSQL_RES* tempResult = NULL;
				sprintf_s(Query, sizeof(Query), "SELECT closingPrice FROM coinshistory WHERE coinName='%s' ORDER BY id DESC LIMIT 1", CoinNames[fetch_index]);
				tempResult = ExcuteQuery(Connect, Query);
				if ((Rows = mysql_fetch_row(tempResult)) != NULL) CoinPrice[fetch_index] = atoi(Rows[0]);
			}

			/* 이전 종가 = 현재가 = 현재 시가 = 현재 종가 = 현재 저가 = 현재 고가 */
			CoinOpeningPrice[fetch_index] = CoinPrice[fetch_index];
			CoinClosingPrice[fetch_index] = CoinPrice[fetch_index];
			CoinLowPrice[fetch_index] = CoinPrice[fetch_index];
			CoinHighPrice[fetch_index] = CoinPrice[fetch_index];
		}


		//while (1)
		//{
		//	sprintf(Query, "SELECT * FROM coinlist"); // 코인 목록 가져오기
		//	excuteStatus = mysql_query(Connect, Query);
		//	if (excuteStatus != 0) printf("Error : %s\n", mysql_error(Connect));
		//	else
		//	{
		//		int i = 0;
		//		Result = mysql_store_result(Connect);
		//		while ((Rows = mysql_fetch_row(Result)) != NULL)
		//		{
		//			sprintf(CoinNames[i], "%s", Rows[0]);
		//			CoinPrice[i] = atoi(Rows[2]);
		//			CoinDelisted[i] = atoi(Rows[3]);
		//			CoinDelistingTerm[i] = atoi(Rows[4]);
		//			sprintf(CoinNews[i], "%s", Rows[5]);
		//			CoinNewsTerm[i] = atoi(Rows[6]);
		//			CoinNewsEffect[i] = atoi(Rows[7]);
		//			i++;
		//		}
		//		mysql_free_result(Result);
		//	}
		//	if (t.tm_min % 5 == 0 && t.tm_min % 10 != 0) IsBackup = false; // %5분마다 backup = false 
		//	if (t.tm_min % 10 == 0 && !IsBackup) // 10분마다 backup = true 
		//	{
		//		//printf("DB-Backup %d%02d%02d_%02d_%dCoins\n", year, month, day, hour, minute);
		//		mysql_select_db(Connect, "coinsdb");
		//		char CreateQuery[600];
		//		sprintf(CreateQuery, "CREATE TABLE %d%02d%02d_%02d_%dCoins (CoinName VARCHAR(20) NOT NULL COLLATE 'euckr_korean_ci', Price INT(11) NULL DEFAULT '0', ClosingPrice INT(11) NULL DEFAULT '0', LowPrice INT(11) NULL DEFAULT '0', HighPrice INT(11) NULL DEFAULT '0', PRIMARY KEY(CoinName) USING BTREE) COLLATE = 'euckr_korean_ci' ENGINE = InnoDB;", year, month, day, hour, minute);
		//		ExcuteQuery(Connect, CreateQuery);
		//		sprintf(Query, "INSERT INTO coinsdb.%d%02d%02d_%02d_%dCoins (CoinName, ClosingPrice) SELECT CoinName, Price FROM coins.coinlist;", year, month, day, hour, minute);
		//		ExcuteQuery(Connect, Query);
		//		for (int i = 0; i < CoinCount; i++)
		//		{
		//			sprintf(Query, "UPDATE %d%02d%02d_%02d_%dCoins SET Price=%d, LowPrice=%d, HighPrice=%d WHERE CoinName='%s';", year, month, day, hour, minute, CoinOpeningPrice[i], CoinLowPrice[i], CoinHighPrice[i], CoinNames[i]);
		//			ExcuteQuery(Connect, Query);
		//			CoinOpeningPrice[i] = CoinPrice[i], CoinLowPrice[i] = CoinPrice[i], CoinHighPrice[i] = CoinPrice[i]; // 시가 = 저가 = 고가 = 현재가로 초기화 
		//		}
		//		sprintf(Query, "%s", "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'coinsdb';");
		//		excuteStatus = mysql_query(Connect, Query);
		//		Result = mysql_store_result(Connect);
		//		Rows = mysql_fetch_row(Result);
		//		int TableCount = atoi(Rows[0]);
		//		while (TableCount > 144)
		//		{
		//			sprintf(Query, "%s", "SHOW TABLES;");
		//			excuteStatus = mysql_query(Connect, Query);
		//			Result = mysql_store_result(Connect);
		//			Rows = mysql_fetch_row(Result);
		//			sprintf(Query, "DROP TABLE %s", Rows[0]);
		//			mysql_free_result(Result);
		//			ExcuteQuery(Connect, Query);
		//			sprintf(Query, "%s", "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'coinsdb';");
		//			excuteStatus = mysql_query(Connect, Query);
		//			Result = mysql_store_result(Connect);
		//			Rows = mysql_fetch_row(Result);
		//			TableCount = atoi(Rows[0]);
		//			mysql_free_result(Result);
		//		}
		//		IsBackup = true;
		//		mysql_select_db(Connect, "coins");
		//	}
		//	if (t.tm_min % 30 == 0 && t.tm_min != 0) IsBackup1 = false; // %30분마다 backup1 = false 
		//	if (t.tm_min == 0 && !IsBackup1) // 1시간마다 backup1 = true 
		//	{
		//		char CreateQuery[600];
		//		//printf("DB-Backup %d%02d%02d_%02dCoins\n", year, month, day, hour);
		//		mysql_select_db(Connect, "coinsdb_hour");
		//		sprintf(CreateQuery, "CREATE TABLE %d%02d%02d_%02dCoins (CoinName VARCHAR(20) NOT NULL COLLATE 'euckr_korean_ci', Price INT(11) NULL DEFAULT '0', ClosingPrice INT(11) NULL DEFAULT '0', LowPrice INT(11) NULL DEFAULT '0', HighPrice INT(11) NULL DEFAULT '0', PRIMARY KEY(CoinName) USING BTREE) COLLATE = 'euckr_korean_ci' ENGINE = InnoDB;", year, month, day, hour);
		//		ExcuteQuery(Connect, CreateQuery);
		//		sprintf(Query, "INSERT INTO coinsdb_hour.%d%02d%02d_%02dCoins (CoinName, ClosingPrice) SELECT CoinName, Price FROM coins.coinlist;", year, month, day, hour);
		//		ExcuteQuery(Connect, Query);
		//		for (int i = 0; i < CoinCount; i++)
		//		{
		//			sprintf(Query, "UPDATE %d%02d%02d_%02dCoins SET Price=%d, LowPrice=%d, HighPrice=%d WHERE CoinName='%s';", year, month, day, hour, CoinOpeningPrice1[i], CoinLowPrice1[i], CoinHighPrice1[i], CoinNames[i]);
		//			ExcuteQuery(Connect, Query);
		//			CoinOpeningPrice1[i] = CoinPrice[i], CoinLowPrice1[i] = CoinPrice[i], CoinHighPrice1[i] = CoinPrice[i]; // 시가 = 저가 = 고가 = 현재가로 초기화 
		//		}
		//		sprintf(Query, "%s", "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'coinsdb_hour';");
		//		excuteStatus = mysql_query(Connect, Query);
		//		Result = mysql_store_result(Connect);
		//		Rows = mysql_fetch_row(Result);
		//		int TableCount = atoi(Rows[0]);
		//		while (TableCount > 920) // 23 * 40 
		//		{
		//			sprintf(Query, "%s", "SHOW TABLES;");
		//			excuteStatus = mysql_query(Connect, Query);
		//			Result = mysql_store_result(Connect);
		//			Rows = mysql_fetch_row(Result);
		//			sprintf(Query, "DROP TABLE %s", Rows[0]);
		//			mysql_free_result(Result);
		//			ExcuteQuery(Connect, Query);
		//			sprintf(Query, "%s", "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'coinsdb_hour';");
		//			excuteStatus = mysql_query(Connect, Query);
		//			Result = mysql_store_result(Connect);
		//			Rows = mysql_fetch_row(Result);
		//			TableCount = atoi(Rows[0]);
		//			mysql_free_result(Result);
		//		}
		//		IsBackup1 = true;
		//		mysql_select_db(Connect, "coins");
		//	}
		//	for (int i = 0; i < CoinCount; i++)
		//	{
		//		int Fluctuation = CoinPrice[i];
		//		if (CoinNewsTerm[i] == 0) sprintf(CoinNews[i], "%s", " ");
		//		if (CoinNewsTerm[i] < 0) CoinNewsTerm[i] = 0; // 뉴스 없을 때 -1
		//		/* 변동 알고리즘 */
		//		// 기본 변동률 적용 : 뉴스 없음 
		//		int NoNews = rand() % 11; // 뉴스 없을 때 변동 확률 
		//		if (NoNews < 3) Fluctuation += 0; // 변동 없음 27% 
		//		else if (NoNews > 6) // 상승 36%
		//		{
		//			if (strcmp(CoinNames[i], "가자코인") == 0) Fluctuation += (rand() % 10) + 1;
		//			if (strcmp(CoinNames[i], "까스") == 0) Fluctuation += (rand() % 20) + 1;
		//			if (strcmp(CoinNames[i], "디카르코") == 0) Fluctuation += (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "람다토큰") == 0) Fluctuation += (rand() % 10) + 1;
		//			if (strcmp(CoinNames[i], "리퍼리음") == 0) Fluctuation += (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "리풀") == 0) Fluctuation += (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "비투코인") == 0) Fluctuation += (rand() % 100) + 1;
		//			if (strcmp(CoinNames[i], "새럼") == 0) Fluctuation += (rand() % 3) + 1;
		//			if (strcmp(CoinNames[i], "서브프레임") == 0) Fluctuation += (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "센드박스") == 0) Fluctuation += (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "솔리나") == 0) Fluctuation += (rand() % 30) + 1;
		//			if (strcmp(CoinNames[i], "스틤") == 0) Fluctuation += (rand() % 7) + 1;
		//			if (strcmp(CoinNames[i], "스틤달러") == 0) Fluctuation += (rand() % 15) + 1;
		//			if (strcmp(CoinNames[i], "승환토큰") == 0) Fluctuation += (rand() % 3) + 1;
		//			if (strcmp(CoinNames[i], "시바코인") == 0) Fluctuation += (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "썸띵") == 0) Fluctuation += (rand() % 6) + 1;
		//			if (strcmp(CoinNames[i], "아르곳") == 0) Fluctuation += (rand() % 6) + 1;
		//			if (strcmp(CoinNames[i], "아이쿠") == 0) Fluctuation += (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "아화토큰") == 0) Fluctuation += (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "앱토수") == 0) Fluctuation += (rand() % 17) + 1;
		//			if (strcmp(CoinNames[i], "에이더") == 0) Fluctuation += (rand() % 15) + 1;
		//			if (strcmp(CoinNames[i], "엑스인피니티") == 0) Fluctuation += (rand() % 20) + 1;
		//			if (strcmp(CoinNames[i], "웨이부") == 0) Fluctuation += (rand() % 12) + 1;
		//			if (strcmp(CoinNames[i], "위밋스") == 0) Fluctuation += (rand() % 16) + 1;
		//			if (strcmp(CoinNames[i], "이더리음") == 0) Fluctuation += (rand() % 70) + 1;
		//			if (strcmp(CoinNames[i], "체인링쿠") == 0) Fluctuation += (rand() % 30) + 1;
		//			if (strcmp(CoinNames[i], "칠리츠") == 0) Fluctuation += (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "코스모수") == 0) Fluctuation += (rand() % 18) + 1;
		//			if (strcmp(CoinNames[i], "폴리콘") == 0) Fluctuation += (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "풀로우") == 0) Fluctuation += (rand() % 18) + 1;
		//			if (strcmp(CoinNames[i], "헌투") == 0) Fluctuation += (rand() % 6) + 1;
		//		}
		//		else // 하락 36%
		//		{
		//			if (strcmp(CoinNames[i], "가자코인") == 0) Fluctuation -= (rand() % 10) + 1;
		//			if (strcmp(CoinNames[i], "까스") == 0) Fluctuation -= (rand() % 20) + 1;
		//			if (strcmp(CoinNames[i], "디카르코") == 0) Fluctuation -= (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "람다토큰") == 0) Fluctuation -= (rand() % 10) + 1;
		//			if (strcmp(CoinNames[i], "리퍼리음") == 0) Fluctuation -= (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "리풀") == 0) Fluctuation -= (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "비투코인") == 0) Fluctuation -= (rand() % 100) + 1;
		//			if (strcmp(CoinNames[i], "새럼") == 0) Fluctuation -= (rand() % 3) + 1;
		//			if (strcmp(CoinNames[i], "서브프레임") == 0) Fluctuation -= (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "센드박스") == 0) Fluctuation -= (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "솔리나") == 0) Fluctuation -= (rand() % 30) + 1;
		//			if (strcmp(CoinNames[i], "스틤") == 0) Fluctuation -= (rand() % 7) + 1;
		//			if (strcmp(CoinNames[i], "스틤달러") == 0) Fluctuation -= (rand() % 15) + 1;
		//			if (strcmp(CoinNames[i], "승환토큰") == 0) Fluctuation -= (rand() % 3) + 1;
		//			if (strcmp(CoinNames[i], "시바코인") == 0) Fluctuation -= (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "썸띵") == 0) Fluctuation -= (rand() % 6) + 1;
		//			if (strcmp(CoinNames[i], "아르곳") == 0) Fluctuation -= (rand() % 6) + 1;
		//			if (strcmp(CoinNames[i], "아이쿠") == 0) Fluctuation -= (rand() % 2) + 1;
		//			if (strcmp(CoinNames[i], "아화토큰") == 0) Fluctuation -= (rand() % 4) + 1;
		//			if (strcmp(CoinNames[i], "앱토수") == 0) Fluctuation -= (rand() % 17) + 1;
		//			if (strcmp(CoinNames[i], "에이더") == 0) Fluctuation -= (rand() % 15) + 1;
		//			if (strcmp(CoinNames[i], "엑스인피니티") == 0) Fluctuation -= (rand() % 20) + 1;
		//			if (strcmp(CoinNames[i], "웨이부") == 0) Fluctuation -= (rand() % 12) + 1;
		//			if (strcmp(CoinNames[i], "위밋스") == 0) Fluctuation -= (rand() % 16) + 1;
		//			if (strcmp(CoinNames[i], "이더리음") == 0) Fluctuation -= (rand() % 70) + 1;
		//			if (strcmp(CoinNames[i], "체인링쿠") == 0) Fluctuation -= (rand() % 30) + 1;
		//			if (strcmp(CoinNames[i], "칠리츠") == 0) Fluctuation -= (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "코스모수") == 0) Fluctuation -= (rand() % 18) + 1;
		//			if (strcmp(CoinNames[i], "폴리콘") == 0) Fluctuation -= (rand() % 11) + 1;
		//			if (strcmp(CoinNames[i], "풀로우") == 0) Fluctuation -= (rand() % 18) + 1;
		//			if (strcmp(CoinNames[i], "헌투") == 0) Fluctuation -= (rand() % 6) + 1;
		//		}
		//		if (CoinNewsEffect[i] != 0)
		//		{
		//			int Percentage = rand() % 5 + 1; // 뉴스 있을 경우, 변동폭 확률 
		//			Fluctuation += ((abs(CoinPrice[i] - Fluctuation) / 2) * Percentage) * CoinNewsEffect[i]; // 뉴스 있음 : 추가 변동률 적용 
		//		}
		//		if (Fluctuation < CoinLowPrice[i]) CoinLowPrice[i] = Fluctuation; // 저가
		//		if (Fluctuation > CoinHighPrice[i]) CoinHighPrice[i] = Fluctuation; // 고가
		//		if (CoinLowPrice[i] < 0) CoinLowPrice[i] = 0; // 저가 0원 보정
		//		if (Fluctuation < CoinLowPrice1[i]) CoinLowPrice1[i] = Fluctuation; // 저가 - 1시간 
		//		if (Fluctuation > CoinHighPrice1[i]) CoinHighPrice1[i] = Fluctuation; // 고가 - 1시간 
		//		if (CoinLowPrice1[i] < 0) CoinLowPrice1[i] = 0; // 저가 0원 보정 - 1시간 
		//		if (CoinDelistingTerm[i] == 0) // 재상장
		//		{
		//			CoinDelisted[i] = 0; // 재상장
		//			CoinNewsEffect[i] = 1; // 재상장시 상승률 보장
		//			sprintf(CoinNews[i], "%s 재상장!", CoinNames[i]);
		//			if (strcmp(CoinNames[i], "가자코인") == 0) CoinPrice[i] = 4000;
		//			if (strcmp(CoinNames[i], "까스") == 0) CoinPrice[i] = 12000;
		//			if (strcmp(CoinNames[i], "디카르코") == 0) CoinPrice[i] = 520;
		//			if (strcmp(CoinNames[i], "람다토큰") == 0) CoinPrice[i] = 200;
		//			if (strcmp(CoinNames[i], "리퍼리음") == 0) CoinPrice[i] = 190;
		//			if (strcmp(CoinNames[i], "리풀") == 0) CoinPrice[i] = 830;
		//			if (strcmp(CoinNames[i], "비투코인") == 0) CoinPrice[i] = 3500000;
		//			if (strcmp(CoinNames[i], "새럼") == 0) CoinPrice[i] = 1100;
		//			if (strcmp(CoinNames[i], "서브프레임") == 0) CoinPrice[i] = 30;
		//			if (strcmp(CoinNames[i], "센드박스") == 0) CoinPrice[i] = 260;
		//			if (strcmp(CoinNames[i], "솔리나") == 0) CoinPrice[i] = 23000;
		//			if (strcmp(CoinNames[i], "스틤") == 0) CoinPrice[i] = 600;
		//			if (strcmp(CoinNames[i], "스틤달러") == 0) CoinPrice[i] = 1200;
		//			if (strcmp(CoinNames[i], "승환토큰") == 0) CoinPrice[i] = 100;
		//			if (strcmp(CoinNames[i], "시바코인") == 0) CoinPrice[i] = 140;
		//			if (strcmp(CoinNames[i], "썸띵") == 0) CoinPrice[i] = 94;
		//			if (strcmp(CoinNames[i], "아르곳") == 0) CoinPrice[i] = 110;
		//			if (strcmp(CoinNames[i], "아이쿠") == 0) CoinPrice[i] = 46;
		//			if (strcmp(CoinNames[i], "아화토큰") == 0) CoinPrice[i] = 148;
		//			if (strcmp(CoinNames[i], "앱토수") == 0) CoinPrice[i] = 8000;
		//			if (strcmp(CoinNames[i], "에이더") == 0) CoinPrice[i] = 260;
		//			if (strcmp(CoinNames[i], "엑스인피니티") == 0) CoinPrice[i] = 8500;
		//			if (strcmp(CoinNames[i], "웨이부") == 0) CoinPrice[i] = 1900;
		//			if (strcmp(CoinNames[i], "위밋스") == 0) CoinPrice[i] = 630;
		//			if (strcmp(CoinNames[i], "이더리음") == 0) CoinPrice[i] = 1250000;
		//			if (strcmp(CoinNames[i], "체인링쿠") == 0) CoinPrice[i] = 11500;
		//			if (strcmp(CoinNames[i], "칠리츠") == 0) CoinPrice[i] = 130;
		//			if (strcmp(CoinNames[i], "코스모수") == 0) CoinPrice[i] = 4000;
		//			if (strcmp(CoinNames[i], "폴리콘") == 0) CoinPrice[i] = 1300;
		//			if (strcmp(CoinNames[i], "풀로우") == 0) CoinPrice[i] = 1050;
		//			if (strcmp(CoinNames[i], "헌투") == 0) CoinPrice[i] = 75;
		//			CoinNewsTerm[i] = 600; // 10분 동안 재상장 알림
		//			sprintf(Query, "UPDATE coinlist SET Price=%d, Delisting=0, DelistingCount=-1, News='%s', NewsContinuous=%d, NewsEffect=%d WHERE CoinName='%s'", CoinPrice[i], CoinNews[i], CoinNewsTerm[i], CoinNewsEffect[i], CoinNames[i]);
		//			ExcuteQuery(Connect, Query);
		//			continue;
		//		}
		//		if (Fluctuation <= 0 && CoinDelisted[i] == 0) // 최초 상장 폐지
		//		{
		//			Fluctuation = 0;
		//			sprintf(CoinNews[i], "%s 상장폐지!", CoinNames[i]);
		//			CoinDelistingTerm[i] = 600 + rand() % 150; // 최소 10분 이상 상폐
		//			CoinNewsTerm[i] = CoinDelistingTerm[i];
		//			sprintf(Query, "UPDATE coinlist SET Price=0, Delisting=1, DelistingCount=%d, News='%s', NewsContinuous=%d, NewsEffect=0 WHERE CoinName='%s'", CoinDelistingTerm[i], CoinNews[i], CoinNewsTerm[i], CoinNames[i]);
		//			ExcuteQuery(Connect, Query);
		//			continue;
		//		}
		//		if (CoinDelisted[i] == 1 && CoinDelistingTerm[i] != -1) // 상장 폐지 중
		//		{
		//			sprintf(Query, "UPDATE coinlist SET Price=0, DelistingCount=%d, NewsContinuous=%d WHERE CoinName='%s' AND Delisting=1", --CoinDelistingTerm[i], --CoinNewsTerm[i], CoinNames[i]);
		//		}
		//		else
		//		{
		//			NewCardIssue(CoinNames[i], CoinNews[i], &CoinNewsTerm[i], &CoinNewsEffect[i]);
		//			sprintf(Query, "UPDATE coinlist SET Price=%d, Delisting=%d, News='%s', NewsContinuous=%d, NewsEffect=%d WHERE CoinName='%s' AND Delisting=0", Fluctuation, CoinDelisted[i], CoinNews[i], --CoinNewsTerm[i], CoinNewsEffect[i], CoinNames[i]);
		//		}
		//		ExcuteQuery(Connect, Query);
		//	}
		//	Sleep(2000); // 2초
		//}
		mysql_close(Connect);
	}
	else printf("DB Connection Fail\n\n");
	return 0;
}

MYSQL_RES* ExcuteQuery(MYSQL* connect, char query[])
{
	int status;
	status = mysql_query(connect, query);

	if (status != 0)
	{
		printf("Error : %s\n", mysql_error(connect));
		return nullptr;
	}
	else
	{
		return mysql_store_result(connect);
	}
}

void BackupData(char names[][30], int opening[], int closing[], int low[], int high[], bool delisted[])
{

}

void NewCardIssue(char* CoinName, char* CoinNews, int* NewsContinuous, int* NewsEffect)
{
	int randomNews = rand() % 5;
	if (strcmp(CoinName, "가자코인") == 0 && *NewsContinuous == 0) // 뉴스가 없을 때
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "가즈아~~~~~~~!!!");
			*NewsEffect = 5;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "으아아아~~~~~~~~~악!!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "가즈아~~~~~~~!!!");
			*NewsEffect = 5;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "으아아아~~~~~~~~~악!!");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0;
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "까스") == 0 && *NewsContinuous == 0) // 뉴스가 없을 때
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "도시까스 가격 인상 계획안 발표!");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "시민들, 까스비 너무 비싸다! 이에 도시까스 가격 인하.");
			*NewsEffect = -2;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "까스공사측, 까스비 안정화 노력...");
			*NewsEffect = 1;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "까스공사 적자 소식.");
			*NewsEffect = -1;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0;
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "디카르코") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "물류센터의 물류 요청 급증!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "디카르코 데이터 센터 화재 발생!");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "새로운 물류 네트워크 기획안 발표.");
			*NewsEffect = 2;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "물류네트워크 수요 증가 소식.");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "람다토큰") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "동영상 플랫폼의 주요 화폐로 람다 토큰 사용을 권장하는 추세.");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "람다 플랫폼, 불건전한 동영상 유포되다!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "람다 플랫폼, 보안 강화 프로젝트 실행.");
			*NewsEffect = 2;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "이번 분기, 동영상 플랫폼 수요량 감소.");
			*NewsEffect = -1;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "리퍼리음") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "새로운 인디 게임사 탄생! 리퍼리음 수요 증가!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "게임 수요 감소에 따른 인디 게임사 마케팅 부담 증가.");
			*NewsEffect = -1;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "모든 게임사의 활발한 마케팅 경쟁!");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "셧다운제 부활 소식에 인디 게임사들 대규모 경영 위기!!");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "리풀") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "리풀코인, 압도적인 거래 속도에 모두가 놀라!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "리풀 코인 송금 타이밍 실패, 이용자의 불만 토로...");
			*NewsEffect = -3;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "블록체인 네트워크에서 “리풀코인”, 성장 가능성 화폐 TOP10에 선정.");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "리풀코인, 갑작스런 하락세 관측.");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "비투코인") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "암호화폐 거래 중 가장 안전한 암호화폐에 “비투코인” 선정.");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "암호화폐 거래 규제, 화폐 가격 하락 우려.");
			*NewsEffect = -1;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "비투코인 끝없는 상승세, “가상화폐는 다 잃어도 되는 사람만 투자해야.”");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "비투코인 붕괴 소식 들려와...");
			*NewsEffect = -5;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "새럼") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ 암호화폐 거래소 이용객 늘어...");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ, “고객 자금 관리 허술하다.” 투자자들 눈물...");
			*NewsEffect = -3;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "세계 4위권 암호화페 거래소에 FTZ 등극.");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "FTZ 파산 위기! 가상화페 몰락의 시작?");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "서브프레임") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "서브프레임, 블록체인 메신저 중 보안성이 매우 뛰어나...");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "서브프레임, 갑작스러운 서버 다운! 현재 원인 파악 중.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "서브프레임의 독자적인 블랙홀 라우팅 알고리즘 개발. “필요한 메시지만 식별 가능하다. 아직은 조금 더 지켜봐야...”");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "사용자가 보낸 메시지를 악의적인 메시지로 인식해, 메시지 전송 오류 현상 발생! 이에 서브프레임은 “없어진 패킷은 복구 불가능하다.”");
			*NewsEffect = -3;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "센드박스") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "블록체인 게임 유행, 센드박스 코인 사용량 급증!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "센드박스 51% 공격으로 이중지불 피해 발생! 알고보니 자신이 만든 게임이 안 팔린 유저들의 분노.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "폭스에딧 이용자 늘어, 센드박스 코인 거래량 증가 전망.");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "모래 상자 안에 모래가 모두 고갈나는 사태 발생! 아이들, 더 이상 모래성 못 만들어 오열... ");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%) 
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "솔리나") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "스틤") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "스틤달러") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "승환토큰") == 0 && *NewsContinuous == 0) // 뉴스가 없을 때
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "“서승환식 포르테” 연비 30.7KM 달성, 하이브리드 차주도 모두 놀라!");
			*NewsEffect = 4;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "서모씨, 음주 단속중 꼬깔콘 쳐 “충격”");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "일본이 절망하고 세계가 열광하며 외신이 주목하는 “서승환식 포르테”");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "서모씨, 포르테 시승해 브랜드 가치 하락!");
			*NewsEffect = -3;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0;
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "시바코인") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "시바코인, 금성갈 때 사용할 필수 코인으로 선언!");
			*NewsEffect = 5;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "일론 머슼흐, 전라도 영광 출신으로 밝혀져... 충격!");
			*NewsEffect = -5;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "일론 머슼흐, “금성 갈끄니까~” 선언.");
			*NewsEffect = 4;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "스페이스에쓰, 로켓 추락 소식.");
			*NewsEffect = -4;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "썸띵") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "아르곳") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "아이쿠") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "아화토큰") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "앱토수") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "앱토수, 삼댜수 판매량 돌파!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "앱토수에서 유해물질 검출! 사람들의 앱토수 거부 반응 증가.");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "오늘의 물건강 소식: “앱토수, 최고의 식수다.”");
			*NewsEffect = 3;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "물 시장 경쟁 증가로 앱토수의 입지 흔들려-...");
			*NewsEffect = -2;
			break;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "에이더") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "엑스인피니티") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "웨이부") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "위밋스") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "이더리음") == 0 && *NewsContinuous == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "가상화폐 채굴 붐! 공장 사장들, 채굴장 사업으로 전환");
			*NewsEffect = 2;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "채굴장 규제 강화, 그래픽카드 시장 포화상태");
			*NewsEffect = -4;
			break;
		case 2:
			sprintf_s(CoinNews, CoinNewsBufferSize, "이더리음, 올해의 가상화폐 TOP3 선정!");
			*NewsEffect = 1;
			break;
		case 3:
			sprintf_s(CoinNews, CoinNewsBufferSize, "블록체인 암호화 기술의 발전, 채굴하기 더 어려워져...");
			*NewsEffect = -4;
		default:
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "체인링쿠") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "칠리츠") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "코스모수") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "폴리콘") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "풀로우") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
	else if (strcmp(CoinName, "헌투") == 0 && *NewsContinuous == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 3 ~ 4 뉴스 없음. 3 / 5 확률로 뉴스 
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
		*NewsContinuous = 300 + rand() % 150; // 5분 이상 지속
	}
}
