<style>
/* PDF로 문서 변환 시에 Page를 강제로 나누기 위해서 사용된다. */
  @media print {
      .pagebreak { page-break-before: always; } /* page-break-after work
  }
</style>



### DeepL 개발 환경 설정

Create: @kimju 2026.09.23

Last Update: @kimju 2026.09.23

---

### Description

본 문서에서는 Aquavisionai 개발 환경을 설명한다. 본 문서의 개발 환경 설정은 의존 라이브러리의 설치 및 구성을 포함한다.

### Environments

OS       : Windows11
Platform : x64
Tool     : Visual Studio 2026 (C++), GCC
Camera   : Basler(GigE)
Dependency Lbrary(static) : OpenCV-4.8.0

### Dependency Library 설정

Opencv등 오픈 소스 라이브러리는 vcpkg를 통해 설치하여 사용을 권장한다.
카메라의 경우 Pylon SDK Firmware를 다운로드 하여 붙이는 것을 권장한다.

##### 1.vcpkg 설치

vcpkg 오픈소스를 다운로드한다.

```sh
git clone https://github.com/microsoft/vcpkg.git
```

bootstrap-vcpkg.bat 를 실행하여 vcpckg 를 빌드한다.

```sh
bootstrap-vcpkg.bat
```

vcpkg 지원 오픈소스를 최신상태로 유지하기 위해 git pull 및 vcpkg 업데이트를 실행한다.

```sh
git pull
vcpkg update
```

##### 2. OpenCV 설치

vcpkg 를 이용하여 opencv를 설치한다.

```sh
vcpkg.exe install opencv[contrib,core,eigen,jpeg,nonfree,opengl,png,tiff]:x64-windows-static-md
```

##### 3. onnxruntime 설치

```sh
https://github.com/microsoft/onnxruntime/releases 에서 1.19.2 버전 다운로드 x64 별도로 존재함
```

##### 4. 환경변수 설정

```sh
예시)
VCPKGX86STATICMDINC = vcpkg설치경로/include
```
