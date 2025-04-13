import socket
import speech_recognition as sr
import pyttsx3  # TTS 라이브러리
from openai import OpenAI
from datetime import datetime, timedelta
import time

# OpenAI API 키 설정
client = OpenAI(api_key="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")

# 음성 인식 및 TTS 엔진 초기화
recognizer = sr.Recognizer()
tts_engine = pyttsx3.init()

# TTS 음성 출력 함수
def speak(text):
    tts_engine.say(text)
    tts_engine.runAndWait()

# OpenAI API를 이용한 답변 생성 함수
def get_openai_response(prompt):
    try:
        completion = client.chat.completions.create(
            model="gpt-4-turbo",
            messages=[{"role": "system", "content": "너는 한국어 비서고 답변은 대화 형식으로만 보내줘. 굳이 강조하려고 글씨 굵게 할 필요 없어 진짜 절대 굵게 하지마"},
                      {"role": "user", "content": prompt}]
        )
        return completion.choices[0].message.content.strip()
    except Exception as e:
        print("❌ OpenAI API 오류:", e)
        return "답변을 생성할 수 없습니다."

# "쿠키야"를 감지할 때까지 대기
def wait_for_activation():
    with sr.Microphone() as source:
        print("\n🟢 대기 중... '쿠키야' 라고 말하면 음성 입력 시작!")
        recognizer.adjust_for_ambient_noise(source)  # 주변 소음 보정
        while True:
            try:
                audio = recognizer.listen(source, timeout=5)
                text = recognizer.recognize_google(audio, language="ko-KR").strip()
                print(f"📝 인식된 문장: {text}")
                if "쿠키야" in text:
                    print("✅ 음성 입력 시작!")
                    speak("네!")  # "네!"라고 응답
                    return True
            except sr.WaitTimeoutError:
                continue  # 타임아웃 시 다시 대기
            except sr.UnknownValueError:
                continue  # 인식 불가 시 다시 대기
            except sr.RequestError:
                print("❌ STT 서비스 오류 발생")
                return False

# 음성을 인식하는 함수
def recognize_speech():
    with sr.Microphone() as source:
        print("🎤 음성을 입력하세요...")
        recognizer.adjust_for_ambient_noise(source)
        try:
            audio = recognizer.listen(source, timeout=5)
            text = recognizer.recognize_google(audio, language="ko-KR").strip()
            print(f"📝 인식된 문장: {text}")
            return text
        except sr.WaitTimeoutError:
            print("⏳ 음성이 감지되지 않았습니다.")
            return None
        except sr.UnknownValueError:
            print("🤷‍♂️ 음성을 인식할 수 없습니다.")
            return None
        except sr.RequestError:
            print("❌ STT 서비스 오류 발생")
            return None

# 날씨 요청을 감지하는 함수
def is_weather_request(user_input):
    weather_keywords = ["날씨", "온도", "비 오니", "비 와", "우산", "더운", "추운", "더워", "추워", "눈 오니", "눈 와"]
    return any(keyword in user_input for keyword in weather_keywords)

def is_schedule_request(text):
    """텍스트가 일정 관련 질문인지 확인"""
    schedule_keywords = ["일정", "스케줄", "약속", "미팅"]
    return any(keyword in text for keyword in schedule_keywords)

def is_kitchen_turtlebot_go(user_input):
    weather_keywords = ["부엌", "주방", "정수기"]
    return any(keyword in user_input for keyword in weather_keywords)

def is_living_turtlebot_go(user_input):
    weather_keywords = ["거실", "현관"]
    return any(keyword in user_input for keyword in weather_keywords)

def is_bed_turtlebot_go(user_input):
    weather_keywords = ["침대", "침실", "안방"]
    return any(keyword in user_input for keyword in weather_keywords)

def is_bath_turtlebot_go(user_input):
    weather_keywords = ["화장실", "욕실"]
    return any(keyword in user_input for keyword in weather_keywords)

def receive_full_response(sock):
    """
    종료 마커를 사용하여 완전한 메시지를 수신하는 함수입니다.
    
    Args:
        sock: 통신 소켓
        
    Returns:
        str: 수신된 전체 메시지
    """
    END_MARKER = "##END##"  # 서버가 전송 끝에 추가한 마커
    buffer = ""
    sock.settimeout(3.0)  # 충분한 타임아웃 설정 (1초)
    
    try:
        while True:
            chunk = sock.recv(4096).decode('utf-8', 'ignore')  # 더 큰 버퍼 사이즈 사용
            if not chunk:
                break  # 연결 종료
            
            buffer += chunk
            
            # 종료 마커가 있는지 확인
            if END_MARKER in buffer:
                # 종료 마커를 제거하고 반환
                buffer = buffer.replace(END_MARKER, "")
                break
    except socket.timeout:
        pass
    except Exception as e:
        print(f"❌ 데이터 수신 중 오류 발생: {e}")
    
    sock.settimeout(None)  # 타임아웃 설정 해제
    return buffer.strip()

def parse_schedule(file_path):
    today = datetime.today().strftime("%Y/%m/%d")
    tomorrow = (datetime.today() + timedelta(days=1)).strftime("%Y/%m/%d")

    schedule_dict = {"오늘": {"높음": [], "중간": [], "낮음": []}, "내일": {"높음": [], "중간": [], "낮음": []}}

    # 파일 읽기
    with open(file_path, "r", encoding="utf-8-sig") as file:
        for line in file:
            print(f"디버깅: {line.strip()}")  # 디버깅용 출력

            parts = line.strip().split(" / ")
            if len(parts) != 4:
                print("⚠️ 형식이 맞지 않음:", parts)  # 디버깅
                continue  # 형식 오류 무시

            raw_date, title, content, priority = parts

            # 날짜 형식 변환 (0이 빠진 경우 보정)
            try:
                date_obj = datetime.strptime(raw_date, "%Y/%m/%d")
                date = date_obj.strftime("%Y/%m/%d")  # 포맷 통일
            except ValueError:
                print("⚠️ 날짜 형식 오류:", raw_date)  # 디버깅
                continue  # 날짜 오류 무시

            # 오늘과 내일 일정만 저장
            if date == today:
                key = "오늘"
            elif date == tomorrow:
                key = "내일"
            else:
                continue  # 오늘과 내일이 아니면 무시

            if priority in ["높음", "중간", "낮음"]:
                schedule_dict[key][priority].append(f"{content}이라는 내용의 {title} 일정")

    # 자연어 형식으로 변환
    result = []
    for day, priorities in schedule_dict.items():
        sentences = []
        for priority, schedules in priorities.items():
            if schedules:
                sentence = f"{priority} 중요도인 일정은 " + ", ".join(schedules)
                sentences.append(sentence)
        if sentences:
            result.append(f"{day}의 일정 중 " + " 그리고 ".join(sentences) + "이 있습니다.")

    return " ".join(result) if result else "오늘과 내일 일정이 없습니다."

def format_weather_response_for_speech(response):
    """
    날씨 응답을 기상캐스터 스타일로 변환하는 함수

    Args:
        response (str): 서버로부터 받은 날씨 문자열
    Returns:
        str: TTS용 말하기 문장
    """
    try:
        lines = response.strip().splitlines()
        if len(lines) < 2:
            return "날씨 정보가 올바르지 않습니다."

        location_info = lines[0].replace("날씨정보 ", "").strip()
        location, time = location_info.split(" - ")
        print(location, end='')
        print(time, end='')
        weather_data = lines[1].strip().split(", ")
        dust_data = lines[2].strip().split(", ")
        print(weather_data, end='')
        print(dust_data)
        # 값 추출
        temp = weather_data[0].split("=")[1].strip()
        sky = weather_data[1].split("=")[1].strip()
        rain = weather_data[2].split("=")[1].strip()
        humidity = weather_data[3].split("=")[1].strip()
        wind = weather_data[4].split("=")[1].split("m")[0].strip()
        pm10 = dust_data[0].split("=")[1].strip()
        pm25 = dust_data[1].split("=")[1].strip()

        # 문장 구성
        sentence = (
            f"{location}의 {time} 날씨입니다. "
            f"현재 기온은 {temp}이고, 하늘은 {sky} 상태입니다. "
            f"강수 확률은 {rain}, 습도는 {humidity}, 바람은 초속 {wind}미터 퍼 세크입니다. "
            f"미세먼지는 {pm10} 수준이며, 초미세먼지 정보는 {pm25}입니다."
        )
        return sentence
    except Exception as e:
        print(f"❌ 날씨 포맷 변환 오류: {e}")
        return "날씨 정보를 처리하는 데 문제가 발생했습니다."
    
def gpt_weather(response):
    sentence = f"{response} 이거를 자연스럽게 말하면서 옷차림 추천도 해줘"
    return sentence

def reconnect_to_server(max_attempts=5, retry_delay=5):
    """
    서버 연결이 끊어졌을 때 재연결을 시도하는 함수
    
    Args:
        max_attempts (int): 최대 재시도 횟수
        retry_delay (int): 재시도 간격(초)
    
    Returns:
        socket or None: 연결된 소켓 객체 또는 실패 시 None
    """
    attempt = 0
    while attempt < max_attempts:
        attempt += 1
        print(f"서버 재연결 시도 {attempt}/{max_attempts}...")
        
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_socket.connect(('10.10.141.73', 5000))
            
            # 장치 이름만 전송 (비밀번호 없음)
            device_id = 'USR_AI'
            client_socket.send(device_id.encode())
            
            # 서버 응답 받기
            response = client_socket.recv(1024).decode()
            print(f"서버 응답: {response}")
            
            if "Error" in response or "Authentication Error" in response:
                print("서버 인증 실패")
                client_socket.close()
                time.sleep(retry_delay)
                continue
                
            print("서버 재연결 성공!")
            return client_socket
            
        except Exception as e:
            print(f"재연결 시도 중 오류 발생: {e}")
            time.sleep(retry_delay)
            
    print(f"{max_attempts}번 재시도했으나 서버에 연결할 수 없습니다.")
    return None

def client_program():
    host = '10.10.141.73'  # 서버 IP 주소
    port = 5000            # 서버 포트 번호

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((host, port))

    # 아이디만 서버로 전송
    device_id = 'USR_AI'
    client_socket.send(device_id.encode())

    # 서버로부터 응답 받기 (아이디 인증 결과)
    response = client_socket.recv(2048).decode().strip()
    print("서버 응답:", response)

    # "쿠키야" 감지 후 음성 입력
    while True:
        if wait_for_activation():  # "쿠키야" 감지
            user_input = recognize_speech()  # 음성 입력 받기
            if user_input is None:
                continue  # 음성 인식 실패 시 다시 대기

            if user_input.lower() == 'exit':  
                print("연결을 종료합니다...")
                speak("연결을 종료합니다.")  # TTS로 안내
                break

            if is_weather_request(user_input):  
                recipient = "USR_API"  
                formatted_message = f"[{recipient}]:날씨\n"
                client_socket.send(formatted_message.encode())
                try:
                    # 개선된 함수를 사용하여 완전한 응답 수신
                    response = receive_full_response(client_socket)
        
                    # 파일에 저장 (디버깅용)
                    with open("weather_response_debug.txt", "w", encoding="utf-8") as f:
                        f.write(response)
                    
                    formatted = gpt_weather(response)
                    openai_response = get_openai_response(formatted)
                    print("📢 변환된 문장:", openai_response)
                    speak(openai_response)
                    with open("output.txt", "w", encoding="utf-8") as file:
                        file.write(formatted)
                except Exception as e:
                    print(f"❌ 응답 수신 및 처리 중 오류 발생: {e}")
                    speak("날씨 정보를 가져오는 데 문제가 발생했습니다.")
            elif is_schedule_request(user_input):
                recipient = "USR_API"
                formatted_message = f"[{recipient}]:일정\n"
                client_socket.send(formatted_message.encode())
                try:
                    # 개선된 함수를 사용하여 완전한 응답 수신
                    response = receive_full_response(client_socket)
        
                    # 파일에 저장 (디버깅용)
                    with open("schedule_response_debug.txt", "w", encoding="utf-8") as f:
                        f.write(response)

                    response = parse_schedule("schedule_response_debug.txt")
                    response = f"{response} 이거를 좀 더 비서가 말하는 것처럼 변형해줘"
                    openai_response = get_openai_response(response)
                    print("📢 변환된 문장:", openai_response)
                    speak(openai_response)
                except Exception as e:
                    print(f"❌ 응답 수신 및 처리 중 오류 발생: {e}")
                    speak("날씨 정보를 가져오는 데 문제가 발생했습니다.")
            elif is_bed_turtlebot_go(user_input):
                recipient = "USR_LIN"  
                formatted_message = f"[{recipient}]BEDGO"
                client_socket.send(formatted_message.encode())
                speak("침실로 이동합니다")
            elif is_bath_turtlebot_go(user_input):
                recipient = "USR_LIN"  
                formatted_message = f"[{recipient}]BATHGO"
                client_socket.send(formatted_message.encode())
                speak("화장실로 이동합니다")
            elif is_living_turtlebot_go(user_input):
                recipient = "USR_LIN"  
                formatted_message = f"[{recipient}]LIVINGGO"
                client_socket.send(formatted_message.encode())
                speak("거실로 이동합니다")
            elif is_kitchen_turtlebot_go(user_input):
                recipient = "USR_LIN"  
                formatted_message = f"[{recipient}]WATERGO"
                client_socket.send(formatted_message.encode())
                speak("부엌으로 이동합니다")    
            else:
                openai_response = get_openai_response(user_input)
                print("🤖 OpenAI 응답:", openai_response)
                speak(openai_response)  # TTS로 읽기

    client_socket.close()

if __name__ == '__main__':
    client_program()
