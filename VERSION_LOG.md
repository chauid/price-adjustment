## price-adjustmente Version Log

### v1.0
> - [ ] C -> C++ 난수 생성기 rand() -> mt19937  
> - [ ] C++형식의 메모리 할당 malloc, free => new, delete  
> - [ ] 

### v0.8
> C++ -> C 마이그레이션 : 메모리 사용량 4MB -> 2MB로 감소  
> DB 접속 정보 환경 변수화 : db.conf  
> DB 스키마 변경

### v0.7
> ### DB 스키마 대규모 변경  
> 1. 저장 방식: 테이블 -> 레코드  
> 2. 백업 테이블 2개-> 1개, 최대 레코드 수 설정
> 
> 변동 알고리즘 변경: 뉴스 효과에 따른 변동률의 최대값은 기본 변동률을 넘을 수 없음.  
> 매 업데이트마다 읽기기능 제거 -> 업데이트 연산만 수행: 데이터 값은 프로그램 메모리에 의존  
> 위 연산으로 부하 감소 -> 변동 업데이트 시간: 4초->2초
> ### hotfix: 비정상적인 종료 수정
> CoinCount 자료형 불일치 수정: unsigned int -> int
> 뉴스 카드 버그 수정 및 프로젝트 작업 디렉토리 수정: '%%' 출력 버그 수정

### v0.6
> 센드박스 뉴스 내용 추가  
> 변동 알고리즘 버그 수정  
> 업데이트 시간 조절 2초->4초(서버 부하 감소)

### v0.5
> 서브프레임 뉴스 내용 추가  
> 이더리움 뉴스Effect 수정-2  
> 업데이트 시간 조절 1초->2초(서버 부하 감소)  
> coinsdb 최대 개수 40->144

### v0.4
> 새럼코인 뉴스 내용 추가  
> 이더리움 뉴스Effect 수정  
> DB_hour 최대 개수 840->920

### ~v.03
> 초기 DB update 로직