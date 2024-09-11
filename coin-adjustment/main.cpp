#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <windows.h> // sed -i 's/#include <windows.h>/#include <unistd.h>/g' {pwd}/coindb.c
#include <mysql.h> // sed -i '/s/#include <mysql.h>/#include \"/usr/include/mysql/mysql.h\"/g' {pwd}/coindb.c

#define VERSION "0.8" // 현재 버전 v0.8

#define READ_MAXCOUNT 50 // 한 줄 최대 입력 수

#define CoinNameBufferSize 30 // 코인 이름 길이: 30byte
#define CoinNewsBufferSize 255 // 코인 뉴스 길이: 255byte
#define QueryBufferSize 512 // 쿼리 길이
#define TickInterval 2000 // 업데이트 대기시간(ms)

/**
 * @brief 쿼리 실행 함수
 * @param MYSQL* Connect 변수, char[] 쿼리
 * @return mysql_store_result(Connect)의 값
 */
MYSQL_RES* ExcuteQuery(MYSQL* connect, const char* query);

/**
 * @brief 뉴스 카드 발급 함수
 * @param 코인ID, 변경 대상 뉴스, 뉴스 지속 시간, 뉴스 영향력
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

	time_t now; // 현재 시간
	struct tm timeInfo; // 시간 정보
	srand((unsigned int)time(NULL));

#pragma region Program Initalize
	printf("CoinDB Version: %s\nmysql client version:%s\n", VERSION, mysql_get_client_info());
	if (!mysql_real_connect(Connect, DBINFO[0], DBINFO[1], DBINFO[2], DBINFO[3], atoi(DBINFO[4]), NULL, 0)) // DB 연결 성공 
	{
		printf("DB Connection Fail\n\n");
		mysql_close(Connect);
		return 0;
	}
	printf("DB Connection Success!\n\n");

	sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins");
	Result = ExcuteQuery(Connect, Query);
	int CoinCount = 0; // 코인 개수
	if ((Rows = mysql_fetch_row(Result)) != NULL) CoinCount = atoi(Rows[0]);
	else
	{
		puts("Program Initalize Error - Check your DB!");
		mysql_close(Connect);
		return 0;
	}

	/* DB data => Heap Memory: 데이터, or * 데이터 버퍼 크기 */
	bool memory_ok = true; // 메모리 할당 상태

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

	/* 초기화 변수 */
	int* CoinOpeningPriceMinute; // 코인 시가 - 10분
	int* CoinLowPriceMinute; // 코인 저가 - 10분
	int* CoinHighPriceMinute; // 코인 고가 - 10분
	int* CoinClosingPriceMinute; // 코인 종가 - 10분

	int* CoinOpeningPriceHour; // 코인 시가 - 1시간
	int* CoinLowPriceHour; // 코인 저가 - 1시간
	int* CoinHighPriceHour; // 코인 고가 - 1시간
	int* CoinClosingPriceHour; // 코인 종가 - 1시간

	/* 메모리 할당: 코인 개수 할당 + 코인 이름 길이 할당 */
	CoinIds = (int*)calloc(CoinCount, sizeof(int)); // CoinIds
	CoinNames = (char**)malloc(sizeof(char*) * CoinCount); // CoinNames
	for (int record_index = 0; record_index < CoinCount; record_index++) CoinNames[record_index] = (char*)calloc(CoinNameBufferSize, sizeof(char));
	CoinPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 가격
	CoinDefaultPrice = (int*)calloc(CoinCount, sizeof(int)); // 코인 기본가
	CoinFluctuationRate = (int*)calloc(CoinCount, sizeof(int)); // 기본 시세 변동률
	CoinNews = (char**)malloc(sizeof(char*) * CoinCount); // 코인뉴스
	for (int record_index = 0; record_index < CoinCount; record_index++) CoinNews[record_index] = (char*)calloc(CoinNewsBufferSize, sizeof(char));
	CoinNewsTerm = (int*)calloc(CoinCount, sizeof(int)); // 코인뉴스기간
	CoinNewsEffect = (int*)calloc(CoinCount, sizeof(int)); // 코인뉴스 영향력
	CoinDelisting = (int*)calloc(CoinCount, sizeof(int)); // 상폐기간
	CoinIsTrade = (int*)calloc(CoinCount, sizeof(int)); // 코인 거래 가능 여부

	CoinOpeningPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // 코인 시가
	CoinOpeningPriceHour = (int*)calloc(CoinCount, sizeof(int)); // 코인 시가
	CoinLowPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // 코인 저가
	CoinLowPriceHour = (int*)calloc(CoinCount, sizeof(int)); // 코인 저가
	CoinHighPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // 코인 고가
	CoinHighPriceHour = (int*)calloc(CoinCount, sizeof(int)); // 코인 고가
	CoinClosingPriceMinute = (int*)calloc(CoinCount, sizeof(int)); // 코인 종가
	CoinClosingPriceHour = (int*)calloc(CoinCount, sizeof(int)); // 코인 종가

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
	/* 코인 데이터 불러오기
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

	/* 프로그램 시작 기준 [시가, 종가, 최고, 최저] = 현재가 */
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
	bool IsBackupMinute = false; // 10분마다 backup - 저장 기간: 1주일
	bool IsBackupHour = false; // 1시간마다 backup - 저장 기간: 2년
	while (true)
	{
		now = time(NULL);
		localtime_s(&timeInfo, &now); // Linux: sed -i 's/localtime_s/localtime_r/g' {pwd}/coindb.c
		if (timeInfo.tm_min % 5 == 0 && timeInfo.tm_min % 10 != 0) IsBackupMinute = false; // %5분마다 backup = false
		if (timeInfo.tm_min % 10 == 0 && !IsBackupMinute) // 10분마다 backup = true
		{
			/* 10분 - {시가, 종가, 저가, 고가} 갱신 및 저장 */
			for (int coin = 0; coin < CoinCount; coin++)
			{
				sprintf_s(Query, QueryBufferSize, "INSERT INTO coins_history_minutely (coin_id, opening_price, closing_price, low_price, high_price, delisted) VALUES (%d, %d, %d, %d, %d, %d)", CoinIds[coin], CoinOpeningPriceMinute[coin], CoinClosingPriceMinute[coin], CoinLowPriceMinute[coin], CoinHighPriceMinute[coin], CoinDelisting[coin]);
				ExcuteQuery(Connect, Query);
				if (Result == NULL) { puts("Update Data Error - 0"); mysql_close(Connect); return 0; }
				/* 현재가 = 현재 시가 = 현재 종가 = 현재 저가 = 현재 고가 초기화 */
				CoinOpeningPriceMinute[coin] = CoinPrice[coin];
				CoinClosingPriceMinute[coin] = CoinPrice[coin];
				CoinLowPriceMinute[coin] = CoinPrice[coin];
				CoinHighPriceMinute[coin] = CoinPrice[coin];
			}

			/* 10분 - 1주일치 데이터 유지 및 삭제 */
			sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins_history_minutely");
			Result = ExcuteQuery(Connect, Query);
			if (Result == NULL) { puts("Update Data Error - 1"); mysql_close(Connect); return 0; }
			if (Rows = mysql_fetch_row(Result))
			{
				if (atoi(Rows[0]) > CoinCount * 6 * 24 * 2)
				{
					sprintf_s(Query, QueryBufferSize, "DELETE FROM coins_history_minutely WHERE id NOT IN (SELECT * FROM (SELECT id FROM coins_history_minutely ORDER BY id DESC LIMIT %d) AS deltable)", CoinCount * 6 * 24 * 2);
					printf("삭제 쿼리: %s\n시각: %d:%d\n", Query, timeInfo.tm_hour, timeInfo.tm_min);
				}
			}
			IsBackupMinute = true;
		}

		if (timeInfo.tm_min > 0 && timeInfo.tm_min < 60) IsBackupHour = false; // 1~59분: backup = false
		if (timeInfo.tm_min == 0 && !IsBackupHour) // 1시간마다 backup = true
		{
			/* 1시간 - {시가, 종가, 저가, 고가} 갱신 및 저장 */
			for (int coin = 0; coin < CoinCount; coin++)
			{
				sprintf_s(Query, QueryBufferSize, "INSERT INTO coins_history_hourly (coin_id, opening_price, closing_price, low_price, high_price, delisted) VALUES (%d, %d, %d, %d, %d, %d)", CoinIds[coin], CoinOpeningPriceHour[coin], CoinClosingPriceHour[coin], CoinLowPriceHour[coin], CoinHighPriceHour[coin], CoinDelisting[coin]);
				ExcuteQuery(Connect, Query);
				if (Result == NULL) { puts("Update Data Error - 2"); mysql_close(Connect); return 0; }
				/* 현재가 = 현재 시가 = 현재 종가 = 현재 저가 = 현재 고가 초기화 */
				CoinOpeningPriceHour[coin] = CoinPrice[coin];
				CoinClosingPriceHour[coin] = CoinPrice[coin];
				CoinLowPriceHour[coin] = CoinPrice[coin];
				CoinHighPriceHour[coin] = CoinPrice[coin];
			}

			/* 1시간 - 2년치 데이터 유지 및 삭제 */
			sprintf_s(Query, QueryBufferSize, "SELECT COUNT(*) FROM coins_history_hourly");
			Result = ExcuteQuery(Connect, Query);
			if (Result == NULL) { puts("Update Data Error - 3"); mysql_close(Connect); return 0; }
			if (Rows = mysql_fetch_row(Result))
			{
				if (atoi(Rows[0]) > CoinCount * 24 * 365 * 2)
				{
					sprintf_s(Query, QueryBufferSize, "DELETE FROM coins_history_hourly WHERE id NOT IN (SELECT * FROM (SELECT id FROM coins_history_hourly ORDER BY id DESC LIMIT %d) AS deltable)", CoinCount * 24 * 365 * 2);
					printf("삭제 쿼리: %s\n시각: %d:%d\n", Query, timeInfo.tm_hour, timeInfo.tm_min);
				}
			}
			IsBackupHour = true;
		}

		/* 가격 변동 및 상장, 뉴스 조정 */
		for (int coin = 0; coin < CoinCount; coin++) ///////////////////////////////// 변동 알고리즘 조정 필요
		{
			// 거래 불가 코인
			if (CoinIsTrade[coin] == 0) continue;

			int resultFluctuation = 0; // 최종 반영 변동률
			int priceFlucDiddle = rand() % 4 + 1; // 기본 변동폭 조정 확률
			int priceDiddle = rand() % 100 + 1; // 시가 변동 확률
			switch (priceFlucDiddle) // 25% 확률
			{
			case 1:
				resultFluctuation = CoinFluctuationRate[coin] / 4; // 기본 변동률의 25%
				break;
			case 2:
				resultFluctuation = CoinFluctuationRate[coin] / 2; // 기본 변동률의 50%
				break;
			case 3:
				resultFluctuation = CoinFluctuationRate[coin] * 3 / 4;  // 기본 변동률의 75%
				break;
			default: // 기본 변동률의 100%
				break;
			}
			resultFluctuation += CoinNewsEffect[coin] * (CoinFluctuationRate[coin] / 5); // 추가 뉴스 변동폭 = 뉴스영향력 * (기본 변동값 * 20%)

			if (priceDiddle > 50) resultFluctuation = resultFluctuation; // 상승률: 35%
			else if (priceDiddle > 50) resultFluctuation = -resultFluctuation; // 하락률: 35%
			else resultFluctuation = 0; // 변동 없음: 30%
			CoinPrice[coin] += resultFluctuation;

			if (CoinPrice[coin] < CoinLowPriceMinute[coin]) CoinLowPriceMinute[coin] = CoinPrice[coin]; // 저가 - 10분
			if (CoinPrice[coin] > CoinHighPriceMinute[coin]) CoinHighPriceMinute[coin] = CoinPrice[coin]; // 고가 - 10분
			if (CoinPrice[coin] < CoinLowPriceHour[coin]) CoinLowPriceHour[coin] = CoinPrice[coin]; // 저가 - 1시간
			if (CoinPrice[coin] > CoinHighPriceHour[coin]) CoinHighPriceHour[coin] = CoinPrice[coin]; // 고가 - 1시간

			// 재상장: 상폐기간 == 1
			if (CoinDelisting[coin] == 1)
			{
				CoinPrice[coin] = (CoinDefaultPrice[coin] + CoinPrice[coin]) / 2;
				sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s 재상장!", CoinNames[coin]);
				CoinNewsTerm[coin] = 3600 / (TickInterval / 1000); // 1시간 재상장 뉴스 지속
				CoinNewsEffect[coin] = 2; // 재상장시 상승률 보장
				CoinDelisting[coin] = 0;
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET price=%d, news='%s', news_term=%d, news_effect=%d, delisting=%d WHERE id=%d", CoinPrice[coin], CoinNews[coin], CoinNewsTerm[coin], CoinNewsEffect[coin], CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// 상장폐지: 현재가 < default_price(-99%) = default_price / 20 && 상폐기간 == 0 (상장중)
			if (CoinPrice[coin] < CoinDefaultPrice[coin] / 100 && CoinDelisting[coin] == 0)
			{
				sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s 상장폐지!", CoinNames[coin]);
				CoinNewsTerm[coin] = 0;
				CoinNewsEffect[coin] = 0;
				CoinDelisting[coin] = 3600 / (TickInterval / 1000) + rand() % (3600 / (TickInterval / 1000)); // 1~2시간 상장폐지
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET news='%s', news_term=0, news_effect=0, delisting=%d WHERE id=%d", CoinNews[coin], CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// 상장폐지 중: 상폐기간 > 1
			if (CoinDelisting[coin] > 1)
			{
				if (CoinDelisting[coin] < 180 / (TickInterval / 1000))
				{
					sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s 재상장 소식 들려와...", CoinNames[coin]);
				}
				sprintf_s(Query, QueryBufferSize, "UPDATE coins SET news='%s', delisting=%d WHERE id=%d", CoinNews[coin], --CoinDelisting[coin], CoinIds[coin]);
				ExcuteQuery(Connect, Query);
				continue;
			}

			// 상장중: 상폐기간 == 0
			if (CoinDelisting[coin] == 0)
			{
				if (CoinNewsTerm[coin] < 1)
				{
					CoinNewsTerm[coin] = 1800 / (TickInterval / 1000) + rand() % 7200 / (TickInterval / 1000); // 30분~2시간 뉴스 지속
					NewsCardIssue(CoinNames[coin], CoinNews[coin], &CoinNewsEffect[coin]); // 뉴스 발급
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
	if (strcmp(CoinName, "가자코인") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "까스") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "디카르코") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "람다토큰") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "리퍼리음") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "리풀") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "비투코인") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "새럼") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "서브프레임") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "센드박스") == 0)
	{
		switch (randomNews)
		{
		case 0:
			sprintf_s(CoinNews, CoinNewsBufferSize, "블록체인 게임 유행, 센드박스 코인 사용량 급증!");
			*NewsEffect = 3;
			break;
		case 1:
			sprintf_s(CoinNews, CoinNewsBufferSize, "센드박스 51%% 공격으로 이중지불 피해 발생! 알고보니 자신이 만든 게임이 안 팔린 유저들의 분노.");
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%) 
			break;
		}
	}
	else if (strcmp(CoinName, "솔리나") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "스틤") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "스틤달러") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "승환토큰") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0;
			break;
		}
	}
	else if (strcmp(CoinName, "시바코인") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "썸띵") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "아르곳") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "아이쿠") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "아화토큰") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "앱토수") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "에이더") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "엑스인피니티") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "웨이부") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "위밋스") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "이더리음") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "체인링쿠") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "칠리츠") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "코스모수") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "폴리콘") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "풀로우") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
	else if (strcmp(CoinName, "헌투") == 0)
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
			sprintf_s(CoinNews, CoinNewsBufferSize, " "); // 4 ~ 5 뉴스 없음. 2 / 3 확률로 뉴스
			*NewsEffect = 0; // 상승세 보장 (상승률:50%)
			break;
		}
	}
}
#pragma endregion
