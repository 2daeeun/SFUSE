# SFUSE
SFUSE는 <b>Simple FUSE</b>의 약자입니다.

파일 읽기 및 삭제, 디렉토리 탐색과 같은 기본적인 파일 시스템의 동작을 <b>FUSE</b>(Filesystem in Userspace)를 사용하여 구현한 간단한 파일 시스템입니다.  

주요 기능 및 코드 설명은 [SFUSE Doxygen](https://2daeeun.github.io/SFUSE/) 문서를 참고바랍니다.  
SFUSE의 버전에 따른 특징은 [HISTORY.md](./) 문서를 참고바랍니다.

#### SFUSE 미리보기
<p align="center">
  <img src="https://raw.githubusercontent.com/2daeeun/SFUSE/refs/heads/main/img/preview.gif">
</p>

---
### 사용 방법
#### 1. 다운로드
이 코드를 실행하기 위한 <b>필요한 패키지 설치</b>와 <b>프로젝트 다운로드</b>를 하는 명령어입니다.
- <b>Ubuntu</b>:
  ```bash
  sudo apt install git build-essential cmake pkg-config fuse3 libfuse3-dev -y && \
  git clone https://github.com/2daeeun/SFUSE.git && \
  cd SFUSE
  ```

- <b>Arch Linux</b>:
  ```bash
  sudo pacman -S git cmake fuse3 && \
  git clone https://github.com/2daeeun/SFUSE.git && \
  cd SFUSE
  ```

#### 2. 코드 컴파일 및 마운트
run.sh는 컴파일과 마운트 작업을 도와주는 셸 스크립트입니다.
  ```bash
./run.sh
```

#### 3. 언마운트 방법
```bash
umount sfuse_filesystem && rmdir sfuse_filesystem
```

---
#### Reference
* **참조한 VSFS 소스코드**
  * [OS-Simple-File-System](https://github.com/leo-tronic/OS-Simple-File-System)
  * [tiny-file-system](https://github.com/macauleyp/tiny-file-system)
  * [fisopfs (Origin)](https://github.com/jmdieguez/fisopfs)
  * [fisopfs (Korean)](https://github.com/2daeeun/fisopfs)

  </br>

* **참조한 VSFS 관련 문서**
  * [**Project 06: Simple File System**: 노터데임 대학교 CSE 30341 운영 체제 원리(2024년 가을)](https://www3.nd.edu/~pbui/teaching/cse.30341.fa18/project06.html)

---
#### NOTE
* 본 프로젝트의 실험은 Arch Linux의 FUSE v3.16.2에서 진행하였으며, Ubuntu 24.04의 FUSE v3.14에서도 작동을 확인하였습니다. (2024년 11월 20일 확인)
* 본 프로젝트는 추후 파일 쓰기와 삭제 기능을 추가 할 예정이며, 더 나아가 I/O 최적화와 로그 시스템 연구에 쓰일 예정입니다.
