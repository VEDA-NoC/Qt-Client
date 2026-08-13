# Qt 개발용 Pi 인증서 위치

Pi의 공개 서버 인증서 `server.crt`만 이 폴더에 복사한다. `server.key`는 Pi 밖으로
복사하지 않는다.

기본 개발 경로는 실행 파일 옆의 `certs/server.crt`다. Qt 설정 화면에서 다른 절대
경로를 선택할 수도 있다. 사이트별 인증서는 저장소에 commit하지 않는다.

Qt는 다음 검증을 모두 수행한다.

- 인증서 trust chain
- 접속 hostname 또는 IP의 SAN
- 선택한 `server.crt` leaf certificate의 SHA-256 pin

인증서 오류를 무시하는 옵션은 제공하지 않는다.
