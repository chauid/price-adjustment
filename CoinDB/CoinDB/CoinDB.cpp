#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h> // windows 환경 build
//#include <unistd.h> // linuxd 환경 build
#include <mysql.h>
//#include "/usr/include/mysql/mysql.h" // linux 환경 build

constexpr auto VERSION = "0.7"; // 현재 버전 v0.7

constexpr int CoinNameBufferSize = 30; // 코인 이름 길이: 30byte
constexpr int CoinNewsBufferSize = 255; // 코인 뉴스 길이: 255byte

/**
 * @brief 쿼리 실행 함수
 * @param MYSQL* Connect 변수, char[] 쿼리
 * @return mysql_store_result(Connect)의 값
 */
MYSQL_RES* ExcuteQuery(MYSQL* connect, char query[]);

/**
 * @brief coinshistory테이블에 INSERT 수행
 * @param 코인명, 시가, 종가, 저가, 고가, 상폐여부
 */
void InsertData(MYSQL* connect, char* query, char* CoinName, int opening, int closing, int low, int high, bool delisted);

/**
 * @brief 뉴스 카드 발급 함수
 * @param 코인명, 변경 대상 뉴스, 뉴스 지속 시간, 뉴스 영향력
 */
void NewCardIssue(char* CoinName, char* CoinNews, int* NewsEffect);

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

	time_t now; // 현재 시간
	struct tm timeInfo; // 시간 정보
	srand((unsigned int)time(NULL));

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

		char** CoinNames; // VARCHAR(30) * CoinCount (Immutable: Read Only)
		int* CoinPrice; // INT * CoinCount
		bool* CoinDelisted; // TINYINT(1): bool * CoinCount
		int* CoinDelistingTerm; // INT * CoinCount
		char** CoinNews; // VARCHAR(255) * CoinCount
		int* CoinNewsTerm; // INT * CoinCount
		int* CoinNewsEffect; // INT * CoinCount
		int* CoinDefaultPrice; // INT * CoinCount (Immutable: Read Only)
		int* CoinFluctuationRate; // INT * CoinCount (Immutable: Read Only)
		int* CoinOrderQuantity; // INT * CoinCount

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
		CoinDefaultPrice = (int*)calloc(CoinCount, sizeof(int)); // CoindefaultPrice
		CoinFluctuationRate = (int*)calloc(CoinCount, sizeof(int)); // CoinFluctuationRate
		CoinOrderQuantity = (int*)calloc(CoinCount, sizeof(int)); // CoinorderQuantity
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
			CoinNews == NULL || CoinNewsTerm == NULL || CoinNewsEffect == NULL || CoinDefaultPrice == NULL ||
			CoinFluctuationRate == NULL || CoinOrderQuantity == NULL || CoinOpeningPrice == NULL || CoinLowPrice == NULL ||
			CoinHighPrice == NULL || CoinClosingPrice == NULL || memory_ok == false)
		{
			puts("Insufficient Memory!");
			free(CoinNames); free(CoinPrice); free(CoinDelisted); free(CoinDelistingTerm);
			free(CoinNews); free(CoinNewsTerm); free(CoinNewsEffect); free(CoinDefaultPrice);
			free(CoinFluctuationRate); free(CoinOrderQuantity); free(CoinOpeningPrice); free(CoinLowPrice);
			free(CoinHighPrice); free(CoinClosingPrice);

			mysql_close(Connect);
			return 0;
		}

		bool IsBackup = false; // 10분마다 backup 

		/* Character Set */
		sprintf_s(Query, sizeof(Query), "SET names euckr");
		ExcuteQuery(Connect, Query);

		/* 코인 데이터 불러오기 */
		sprintf_s(Query, sizeof(Query), "SELECT coinName, price, delisted, delistingTerm, news, newsTerm, newsEffect, defaultPrice, fluctuationRate, orderQuantity FROM coins");
		Result = ExcuteQuery(Connect, Query);
		for (int fetch_index = 0; fetch_index < CoinCount; fetch_index++)
		{
			if ((Rows = mysql_fetch_row(Result)) != NULL)
			{
				sprintf_s(CoinNames[fetch_index], CoinNameBufferSize, "%s", Rows[0]);

				if (CoinPrice[fetch_index] == 0 && atoi(Rows[2]) != 1)
				{// 현재가 == 0 && 상폐X => 이전 종가 대신 현재가로 적용, [시가, 종가, 저가, 고가] = 현재가로 적용(history 테이블에서 데이터를 불러오지 못 할 경우)
					CoinPrice[fetch_index] = atoi(Rows[1]);
					CoinOpeningPrice[fetch_index] = CoinPrice[fetch_index];
					CoinClosingPrice[fetch_index] = CoinPrice[fetch_index];
					CoinLowPrice[fetch_index] = CoinPrice[fetch_index];
					CoinHighPrice[fetch_index] = CoinPrice[fetch_index];
				}
				CoinDelisted[fetch_index] = atoi(Rows[2]);
				CoinDelistingTerm[fetch_index] = atoi(Rows[3]);
				sprintf_s(CoinNews[fetch_index], CoinNewsBufferSize, "%s", Rows[4]);
				CoinNewsTerm[fetch_index] = atoi(Rows[5]);
				CoinNewsEffect[fetch_index] = atoi(Rows[6]);
				CoinDefaultPrice[fetch_index] = atoi(Rows[7]);
				CoinFluctuationRate[fetch_index] = atoi(Rows[8]);
				CoinOrderQuantity[fetch_index] = atoi(Rows[9]);
			}
			else puts("DB의 코인 개수를 확인해주세요");
		}

		/* 이전 가격 데이터 불러오기: coinshistory테이블의 id 역순 */
		sprintf_s(Query, sizeof(Query), "SELECT COUNT(*) FROM coinshistory");
		Result = ExcuteQuery(Connect, Query);
		if (Rows = mysql_fetch_row(Result))
		{
			if (atoi(Rows[0]) >= CoinCount)
			{
				for (int fetch_index = 0; fetch_index < CoinCount; fetch_index++)
				{
					sprintf_s(Query, sizeof(Query), "SELECT closingPrice FROM coinshistory WHERE coinName='%s' ORDER BY id DESC LIMIT 1", CoinNames[fetch_index]);
					Result = ExcuteQuery(Connect, Query);
					if (Rows = mysql_fetch_row(Result)) CoinPrice[fetch_index] = atoi(Rows[0]);

					/* 이전 종가 = 현재가 = 현재 시가 = 현재 종가 = 현재 저가 = 현재 고가 */
					CoinOpeningPrice[fetch_index] = CoinPrice[fetch_index];
					CoinClosingPrice[fetch_index] = CoinPrice[fetch_index];
					CoinLowPrice[fetch_index] = CoinPrice[fetch_index];
					CoinHighPrice[fetch_index] = CoinPrice[fetch_index];
				}
			}
		}

		while (true)
		{
			now = time(NULL);
			localtime_s(&timeInfo, &now);
			if (timeInfo.tm_min % 5 == 0 && timeInfo.tm_min % 10 != 0) IsBackup = false; // %5분마다 backup = false 
			if (timeInfo.tm_min % 10 == 0 && !IsBackup) // 10분마다 backup = true 
			{
				for (int coin = 0; coin < CoinCount; coin++)
				{
					InsertData(Connect, Query, CoinNames[coin], CoinOpeningPrice[coin], CoinClosingPrice[coin], CoinLowPrice[coin], CoinHighPrice[coin], CoinDelisted[coin]);
					/* 현재가 = 현재 시가 = 현재 종가 = 현재 저가 = 현재 고가 초기화 */
					CoinOpeningPrice[coin] = CoinPrice[coin];
					CoinClosingPrice[coin] = CoinPrice[coin];
					CoinLowPrice[coin] = CoinPrice[coin];
					CoinHighPrice[coin] = CoinPrice[coin];
				}

				/* 2년치 데이터 저장 */
				sprintf_s(Query, sizeof(Query), "SELECT COUNT(*) FROM coinshistory");
				Result = ExcuteQuery(Connect, Query);
				if (Rows = mysql_fetch_row(Result))
				{
					if (atoi(Rows[0]) > 4200000)
					{
						printf("삭제");
					}
				}
				IsBackup = true;
			}

			for (int coin = 0; coin < CoinCount; coin++)
			{
				int newsEffectValue = 0; // 코인 뉴스에 따른 추가 변동폭
				int defaultFluctuationValue = CoinFluctuationRate[coin]; // 기본 변동폭 조정
				int priceFlucDiddle = rand() % 4 + 1; // 기본 변동폭 조정 확률
				int priceDiddle = rand() % 100 + 1; // 시가 변동 확률
				switch (priceFlucDiddle) // 25% 확률
				{
				case 1:
					defaultFluctuationValue = defaultFluctuationValue / 4; // 기본 변동률의 25%
					break;
				case 2:
					defaultFluctuationValue = defaultFluctuationValue / 2; // 기본 변동률의 50%
					break;
				case 3:
					defaultFluctuationValue = defaultFluctuationValue * 3 / 4;  // 기본 변동률의 75%
					break;
				default:  // 기본 변동률의 100%
					break;
				}
				newsEffectValue = CoinNewsEffect[coin] * (CoinFluctuationRate[coin] / 5); // 추가 뉴스 변동폭 = 뉴스영향력 * (기본 변동값 * 20%)

				if (priceDiddle > 65) CoinPrice[coin] += defaultFluctuationValue + newsEffectValue; // 상승률: 35%
				else if (priceDiddle > 30) CoinPrice[coin] -= defaultFluctuationValue - newsEffectValue; // 하락률: 35%
				// 변동 없음: 30%

				if (CoinPrice[coin] < CoinLowPrice[coin]) CoinLowPrice[coin] = CoinPrice[coin]; // 저가
				if (CoinPrice[coin] > CoinHighPrice[coin]) CoinHighPrice[coin] = CoinPrice[coin]; // 고가

				// 재상장: 상폐기간 == 0 && 상장폐지 상태 == true (1회성)
				if (CoinDelisted[coin] == 1 && CoinDelistingTerm[coin] == 0)
				{
					CoinPrice[coin] = CoinDefaultPrice[coin];
					CoinDelisted[coin] = 0;
					CoinDelistingTerm[coin] = -1;
					CoinNewsEffect[coin] = 2; // 재상장시 상승률 보장
					sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s 재상장!", CoinNames[coin]);
					CoinNewsTerm[coin] = 1800; // 1시간 재상장 뉴스 지속
					sprintf_s(Query, sizeof(Query), "UPDATE coins SET price=%d, delisted=0, delistingTerm=-1, news='%s', newsTerm=%d, newsEffect=%d WHERE coinName='%s'", CoinDefaultPrice[coin], CoinNews[coin], CoinNewsTerm[coin], CoinNewsEffect[coin], CoinNames[coin]);
					ExcuteQuery(Connect, Query);
					continue;
				}

				// 상장폐지: 현재가 < 1 && 상장폐지 상태 == false (1회성)
				if (CoinPrice[coin] < 1 && CoinDelisted[coin] == 0)
				{
					CoinPrice[coin] = 0;
					CoinDelisted[coin] = 1; // 상장폐지
					CoinDelistingTerm[coin] = 1800 + rand() % 1800; // 1~2시간 상장폐지
					CoinNewsEffect[coin] = 0;
					sprintf_s(CoinNews[coin], CoinNewsBufferSize, "%s 상장폐지!", CoinNames[coin]);
					CoinNewsTerm[coin] = CoinDelistingTerm[coin];
					sprintf_s(Query, sizeof(Query), "UPDATE coins SET price=0, delisted=1, delistingTerm=%d, news='%s', newsTerm=%d, newsEffect=0 WHERE coinName='%s'", CoinDelistingTerm[coin], CoinNews[coin], CoinNewsTerm[coin], CoinNames[coin]);
					ExcuteQuery(Connect, Query);
					continue;
				}

				// 상장 폐지 중: 상장폐지 상태 == true
				if (CoinDelisted[coin] == 1)
				{
					sprintf_s(Query, sizeof(Query), "UPDATE coins SET delistingTerm=%d, newsTerm=%d WHERE coinName='%s'", --CoinDelistingTerm[coin], --CoinNewsTerm[coin], CoinNames[coin]);
					ExcuteQuery(Connect, Query);
					continue;
				}
				if (CoinDelistingTerm[coin] < -1) CoinDelistingTerm[coin] = -1; // 상폐기간 -1 보정

				// 상장중: 상장폐지 상태 == false && 코인 가격 > 0
				if (CoinDelisted[coin] == 0 && CoinPrice[coin] > 0)
				{
					if (CoinNewsTerm[coin] < 1)
					{
						CoinNewsTerm[coin] = 900 + rand() % 2700; // 30분~2시간 뉴스 지속
						NewCardIssue(CoinNames[coin], CoinNews[coin], &CoinNewsEffect[coin]); // 뉴스 발급
					}
					sprintf_s(Query, sizeof(Query), "UPDATE coins SET price=%d, news='%s', newsTerm=%d, newsEffect=%d WHERE coinName='%s'", CoinPrice[coin], CoinNews[coin], --CoinNewsTerm[coin], CoinNewsEffect[coin], CoinNames[coin]);
					ExcuteQuery(Connect, Query);
				}
			}
			Sleep(2000);
		}
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

void InsertData(MYSQL* connect, char* query, char* CoinName, int opening, int closing, int low, int high, bool delisted)
{
	sprintf_s(query, 400, "INSERT INTO coinshistory(coinName, historyTime, openingPrice, closingPrice, lowPrice, highPrice, delisted) VALUES('%s', NOW(), %d, %d, %d, %d, %d)", CoinName, opening, closing, low, high, (int)delisted);
	ExcuteQuery(connect, query);
}

void NewCardIssue(char* CoinName, char* CoinNews, int* NewsEffect)
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
