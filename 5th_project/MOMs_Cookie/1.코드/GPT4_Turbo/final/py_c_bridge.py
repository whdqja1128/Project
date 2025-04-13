 #!/usr/bin/env python3
"""
날씨와 미세먼지 정보를 통합하여 파일로 저장하는 브릿지 스크립트
Python 날씨 수집 모듈과 C 클라이언트 사이에서 파일로 데이터를 교환합니다.
ID만 사용하는 버전으로 인증 시 비밀번호를 사용하지 않습니다.
AI 클라이언트와의 통신 지원이 추가되었습니다.
"""

import requests
import urllib.parse
import pandas as pd
import json
import os
import time
import socket
from datetime import datetime, timedelta

###############################################
# 설정
###############################################

# API 키 설정
WEATHER_API_KEY = "z86z+0I1JcH4vn38OAzwmDobdVHRIvXiE2eHgGQtzDiMGMTx2VOmNew1QefS3L3X1uKJG3y/Tl7SRWkQJ4OZeQ=="

# 출력 파일 설정
WEATHER_FILE = "weather_data.txt"  # C 클라이언트가 읽는 파일

# 업데이트 간격 (초)
UPDATE_INTERVAL = 900  # 15분

# 위치 정보 설정
DEFAULT_LOCATION = "경기 수원시"  # 기본 위치

# 서버 설정 (직접 전송 옵션)
SERVER_IP = '10.10.141.73'  # 서버 IP 주소
SERVER_PORT = 5000       # 서버 포트 번호 
DEVICE_NAME = "USR_API"  # 장치 이름 (USR_ 형식으로 통일)

###############################################
# 서버 연결 관련 함수
###############################################

def connect_to_server():
    """서버에 직접 연결하는 함수"""
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((SERVER_IP, SERVER_PORT))
        
        # 장치 이름만 전송 (비밀번호 없음)
        client_socket.send(DEVICE_NAME.encode())
        
        # 서버 응답 받기
        response = client_socket.recv(1024).decode()
        print(f"서버 응답: {response}")

        # 날씨 요청 명령 처리 감시
        if "WEATHER_REQ" in response:
            handle_weather_request(client_socket)
        
        if "Error" in response or "Authentication Error" in response:
            print("서버 인증 실패")
            client_socket.close()
            return None
            
        return client_socket
    except Exception as e:
        print(f"서버 연결 오류: {e}")
        return None

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
            client_socket.connect((SERVER_IP, SERVER_PORT))
            
            # 장치 이름만 전송 (비밀번호 없음)
            client_socket.send(DEVICE_NAME.encode())
            
            # 서버 응답 받기
            response = client_socket.recv(1024).decode()
            print(f"서버 응답: {response}")
            
            # 날씨 요청 명령 처리 감시
            if "WEATHER_REQ" in response:
                handle_weather_request(client_socket)
            
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

def handle_weather_request(sock):
    """날씨 요청 처리 함수"""
    # 실시간으로 경기 수원시 날씨 정보 가져오기
    combined_data = update_weather_data(DEFAULT_LOCATION, False)
    
    # 서버에 전송
    if sock is not None:
        send_to_server(sock, combined_data)
        print("날씨 요청에 응답했습니다.")

def send_to_server(sock, message, recipient="USR_AI"):
    """서버에 직접 메시지를 보내는 함수"""
    if sock is None:
        print("서버 연결이 없습니다.")
        return False
    
    try:
        # 메시지 끝에 종료 마커 추가
        message_with_marker = message + "##END##"
        
        # 전체 메시지를 일관된 형식으로 포맷팅
        formatted_msg = f"{recipient}:{message_with_marker}"
        
        # UTF-8로 인코딩
        encoded_msg = formatted_msg.encode('utf-8')
        
        # 메시지 전송 (길이 제한 제거)
        sock.send(encoded_msg)
        
        print(f"전송된 메시지 (종료 마커 포함, 길이: {len(encoded_msg)}바이트)")
        
        # 서버 응답 받기
        try:
            sock.settimeout(2.0)  # 2초 타임아웃
            response = sock.recv(1024).decode('utf-8')
            print(f"서버 응답: {response}")
            sock.settimeout(None)  # 타임아웃 해제
        except socket.timeout:
            print("서버 응답 타임아웃")
        
        return True
    except Exception as e:
        print(f"메시지 전송 오류: {e}")
        return False

# 메시지 모니터링 및 응답 함수
# 메시지 감지 로직 개선
def monitor_messages(client_socket):
    """
    서버로부터 메시지를 모니터링하고 필요한 응답을 보내는 함수
    소켓 상태를 점검하고 연결 문제 감지 기능 추가
    날씨 및 일정 명령어 처리
    
    Returns:
        bool: 성공적으로 모니터링했으면 True, 오류 발생 시 False
    """
    try:
        if client_socket is None or client_socket.fileno() == -1:
            return False
            
        client_socket.settimeout(1.0)
        try:
            data = client_socket.recv(1024).decode('utf-8', 'ignore')
            if not data:  # 연결이 끊어진 경우
                print("서버로부터 빈 데이터 수신. 연결이 끊어진 것으로 판단됩니다.")
                return False
                
            print(f"수신된 원본 메시지: {data}")  # 디버깅용
            
            # 발신자 추출 (메시지 형식: [발신자]명령어)
            sender = "USR_AI"  # 기본값
            if '[' in data and ']' in data:
                start_idx = data.find('[') + 1
                end_idx = data.find(']')
                if start_idx < end_idx:
                    sender = data[start_idx:end_idx]
            
            # 날씨 요청 패턴 검사
            weather_detected = '날씨' in data
            
            # 일정 요청 패턴 검사 (새로 추가)
            schedule_detected = '일정' in data
            
            if weather_detected:
                # 날씨 정보 실시간 수집
                weather_data = update_weather_data(DEFAULT_LOCATION, False)
                
                # 발신자에게 응답
                response = send_to_server(client_socket, weather_data, sender)
                print(f"{sender}에게 날씨 정보 전송 완료: {response}")
                return True
                
            elif schedule_detected:
                # 일정 정보 요청 처리 (새로 추가)
                schedule_data = get_schedule_data()
                
                # 발신자에게 응답
                response = send_to_server(client_socket, schedule_data, sender)
                print(f"{sender}에게 일정 정보 전송 완료: {response}")
                return True
            
        except socket.timeout:
            pass  # 타임아웃은 정상적인 경우
        except ConnectionResetError:
            print("서버와의 연결이 재설정되었습니다.")
            return False
        except BrokenPipeError:
            print("파이프가 끊어졌습니다. 서버와의 연결이 종료되었습니다.")
            return False
        except Exception as e:
            print(f"메시지 모니터링 중 오류: {str(e)}")
            return False
        
        client_socket.settimeout(None)
        return True
    except Exception as e:
        print(f"메시지 모니터링 전체 오류: {str(e)}")
        return False

# 일정 정보를 가져오는 함수 추가
def get_schedule_data():
    """
    schedule.txt 파일에서 일정 정보를 읽어오는 함수
    """
    SCHEDULE_FILE = "schedule.txt"
    
    try:
        # 일정 파일이 있는지 확인
        if not os.path.exists(SCHEDULE_FILE):
            print(f"일정 파일({SCHEDULE_FILE})이 없습니다.")
            return "일정정보\n일정 정보가 없습니다. 앱에서 먼저 일정을 동기화해주세요."
        
        # 파일에서 일정 정보 읽기
        with open(SCHEDULE_FILE, 'r', encoding='utf-8') as f:
            schedule_data = f.read()
        
        # 빈 파일인 경우
        if not schedule_data.strip():
            return "일정정보\n일정 정보가 없습니다."
        
        # 현재 날짜 가져오기
        now = datetime.now()
        current_time = now.strftime("%Y년 %m월 %d일 %H시 %M분")
        
        # 결과 문자열 구성 - 일정정보 헤더 추가
        result = f"일정정보\n{current_time} 기준\n\n{schedule_data}"
        return result
        
    except Exception as e:
        print(f"일정 정보 처리 오류: {e}")
        return f"일정정보\n일정 정보를 가져오는 중 오류가 발생했습니다: {str(e)}"

###############################################
# 지역 정보 관련 함수
###############################################

def get_grid_location():
    """
    전국 주요 지역의 기상청 격자 좌표를 제공하는 함수
    
    Returns:
        DataFrame: 지역명과 해당 격자 좌표(nx, ny)가 포함된 데이터프레임
    """
    # 주요 지역 격자 좌표 정보
    grid_locations = [
        {'지역': '서울', '행정구역': '중구', 'nx': 60, 'ny': 127},
        {'지역': '서울', '행정구역': '강남구', 'nx': 61, 'ny': 125},
        {'지역': '서울', '행정구역': '서초구', 'nx': 61, 'ny': 125},
        {'지역': '서울', '행정구역': '송파구', 'nx': 62, 'ny': 126},
        {'지역': '서울', '행정구역': '강서구', 'nx': 58, 'ny': 126},
        {'지역': '부산', '행정구역': '중구', 'nx': 97, 'ny': 74},
        {'지역': '부산', '행정구역': '해운대구', 'nx': 99, 'ny': 75},
        {'지역': '대구', '행정구역': '중구', 'nx': 89, 'ny': 90},
        {'지역': '인천', '행정구역': '중구', 'nx': 55, 'ny': 124},
        {'지역': '광주', '행정구역': '북구', 'nx': 58, 'ny': 74},
        {'지역': '대전', '행정구역': '중구', 'nx': 67, 'ny': 100},
        {'지역': '울산', '행정구역': '중구', 'nx': 102, 'ny': 84},
        {'지역': '세종', '행정구역': '세종특별자치시', 'nx': 66, 'ny': 103},
        {'지역': '경기', '행정구역': '수원시', 'nx': 60, 'ny': 121},
        {'지역': '경기', '행정구역': '고양시', 'nx': 57, 'ny': 128},
        {'지역': '강원', '행정구역': '춘천시', 'nx': 73, 'ny': 134},
        {'지역': '충북', '행정구역': '청주시', 'nx': 69, 'ny': 107},
        {'지역': '충남', '행정구역': '천안시', 'nx': 63, 'ny': 110},
        {'지역': '전북', '행정구역': '전주시', 'nx': 63, 'ny': 89},
        {'지역': '전남', '행정구역': '목포시', 'nx': 50, 'ny': 67},
        {'지역': '경북', '행정구역': '포항시', 'nx': 102, 'ny': 94},
        {'지역': '경남', '행정구역': '창원시', 'nx': 90, 'ny': 77},
        {'지역': '제주', '행정구역': '제주시', 'nx': 52, 'ny': 38}
    ]
    
    return pd.DataFrame(grid_locations)

def get_sido_list():
    """
    에어코리아 API에서 사용 가능한 시도 목록을 반환합니다.
    """
    sido_list = [
        '서울', '부산', '대구', '인천', '광주', '대전', '울산', '경기', 
        '강원', '충북', '충남', '전북', '전남', '경북', '경남', '제주', '세종'
    ]
    return sido_list

def location_name_to_sido(location_name):
    """
    지역명을 시도명으로 변환하는 함수
    
    Args:
        location_name (str): 지역명 또는 시도명
    
    Returns:
        str: 시도명 (서울, 부산 등)
    """
    # 시도 목록
    sido_list = get_sido_list()
    
    # 지역명이 시도명 포함하는지 확인
    for sido in sido_list:
        if sido in location_name:
            return sido
    
    # 행정구역으로 시도 찾기
    grid_df = get_grid_location()
    result = grid_df[grid_df['행정구역'].str.contains(location_name, case=False)]
    
    if len(result) > 0:
        return result.iloc[0]['지역']
    
    # 못 찾으면 기본값 반환
    return '서울'

def find_location_grid(location_name=None):
    """
    지역명으로 기상청 격자 좌표를 찾아주는 함수
    
    Args:
        location_name (str, optional): 찾고자 하는 지역명. 
                                      입력하지 않으면 전체 목록을 출력합니다.
    
    Returns:
        tuple or DataFrame: 지역명이 주어진 경우 해당 지역의 (nx, ny) 좌표 반환,
                          지역명이 없으면 전체 데이터프레임 반환
    """
    # 지역 목록 가져오기
    grid_df = get_grid_location()
    
    if location_name is None:
        print("전체 지역 목록 출력:")
        return grid_df, None
    
    # 검색 방법 개선: 공백으로 분리하여 각 단어가 포함된 결과 검색
    parts = location_name.split()
    if len(parts) > 1:
        # 여러 단어로 구성된 검색어일 경우 (예: '경기 수원시')
        # 첫 번째 단어는 지역에서 검색 (예: '경기')
        # 두 번째 이후 단어는 행정구역에서 검색 (예: '수원시')
        region_part = parts[0]
        district_part = ' '.join(parts[1:])
        
        result = grid_df[(grid_df['지역'].str.contains(region_part, case=False)) & 
                         (grid_df['행정구역'].str.contains(district_part, case=False))]
    else:
        # 단일 단어 검색일 경우 기존 방식 유지
        result = grid_df[grid_df['지역'].str.contains(location_name, case=False) | 
                        grid_df['행정구역'].str.contains(location_name, case=False)]
    
    if len(result) == 0:
        print(f"'{location_name}'에 해당하는 지역이 없습니다.")
        print("기본 지역(서울 중구)을 사용합니다.")
        result = grid_df[(grid_df['지역'] == '서울') & (grid_df['행정구역'] == '중구')]
        
    if len(result) == 1:
        nx = result.iloc[0]['nx']
        ny = result.iloc[0]['ny']
        region = result.iloc[0]['지역']
        district = result.iloc[0]['행정구역']
        print(f"{region} {district}의 격자 좌표: nx={nx}, ny={ny}")
        location_info = {
            '지역': region,
            '행정구역': district,
            'nx': nx,
            'ny': ny
        }
        return (nx, ny), location_info
    else:
        # 여러 결과가 나온 경우 첫 번째 결과 사용
        nx = result.iloc[0]['nx']
        ny = result.iloc[0]['ny']
        region = result.iloc[0]['지역']
        district = result.iloc[0]['행정구역']
        print(f"여러 결과 중 첫 번째를 선택: {region} {district} (nx={nx}, ny={ny})")
        location_info = {
            '지역': region,
            '행정구역': district,
            'nx': nx,
            'ny': ny
        }
        return (nx, ny), location_info

###############################################
# 날씨 정보 관련 함수
###############################################

def get_weather_data(nx, ny):
    """
    기상청 API를 통해 날씨 정보를 가져옵니다.
    
    Args:
        nx (int): 기상청 좌표 X
        ny (int): 기상청 좌표 Y
    
    Returns:
        dict: 날씨 정보 데이터
    """
    # URL 인코딩된 서비스 키 준비
    encoded_service_key = urllib.parse.quote_plus(WEATHER_API_KEY)
    
    # 현재 날짜와 시간 정보 가져오기
    now = datetime.now()
    base_date = now.strftime("%Y%m%d")  # 오늘 날짜 YYYYMMDD 형식
    
    # 현재 시간에 가장 가까운 발표 시간 계산
    hour = now.hour
    # 발표 시간은 02, 05, 08, 11, 14, 17, 20, 23시로 설정
    base_times = [2, 5, 8, 11, 14, 17, 20, 23]
    base_time = 2  # 기본값
    
    for t in base_times:
        if hour >= t:
            base_time = t
        else:
            break
    
    # API 요청에 필요한 파라미터 설정
    base_time_str = f"{base_time:02d}00"  # 0200, 0500, 0800, ... 형식
    
    url = "http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getVilageFcst"
    params = {
        'serviceKey': WEATHER_API_KEY,
        'numOfRows': '500',  # 더 많은 결과를 가져오기 위해 증가
        'pageNo': '1',
        'dataType': 'JSON',
        'base_date': base_date,
        'base_time': base_time_str,
        'nx': str(nx),  # 좌표 X
        'ny': str(ny)   # 좌표 Y
    }
    
    try:
        # API 요청
        response = requests.get(url, params=params)
        
        # 응답 확인
        if response.status_code == 200:
            data = response.json()
            
            # 응답 결과 확인
            result_code = data['response']['header']['resultCode']
            
            if result_code == '00':  # 정상 응답
                items = data['response']['body']['items']['item']
                print(f"날씨 정보: 총 {len(items)}개의 예보 항목을 가져왔습니다.")
                return data
            else:
                print(f"날씨 API 응답 오류: {data['response']['header']['resultMsg']}")
                return None
        else:
            print(f"날씨 API HTTP 오류: {response.status_code}")
            return None
            
    except Exception as e:
        print(f"날씨 API 예외 발생: {e}")
        return None

def parse_weather_data(data):
    """
    날씨 데이터를 분석하여 보기 쉽게 정리합니다.
    
    Args:
        data (dict): API 응답 데이터
    
    Returns:
        DataFrame: 정리된 날씨 정보
    """
    if data is None:
        return None
    
    items = data['response']['body']['items']['item']
    
    # 날씨 코드 해석을 위한 딕셔너리
    sky_code = {
        '1': '맑음',
        '3': '구름많음',
        '4': '흐림'
    }
    
    pty_code = {
        '0': '없음',
        '1': '비',
        '2': '비/눈',
        '3': '눈',
        '4': '소나기'
    }
    
    # 날씨 정보를 담을 딕셔너리
    weather_info = {}
    
    for item in items:
        # 예보 일자 및 시간
        forecast_date = item['fcstDate']
        forecast_time = item['fcstTime']
        category = item['category']
        value = item['fcstValue']
        
        # 날짜와 시간을 키로 사용
        key = f"{forecast_date}_{forecast_time}"
        
        if key not in weather_info:
            weather_info[key] = {
                '날짜': forecast_date,
                '시간': forecast_time
            }
        
        # 카테고리별 처리
        if category == 'TMP':  # 기온
            weather_info[key]['기온(°C)'] = value
        elif category == 'POP':  # 강수확률
            weather_info[key]['강수확률(%)'] = value
        elif category == 'PTY':  # 강수형태
            weather_info[key]['강수형태'] = pty_code.get(value, value)
        elif category == 'SKY':  # 하늘상태
            weather_info[key]['하늘상태'] = sky_code.get(value, value)
        elif category == 'REH':  # 습도
            weather_info[key]['습도(%)'] = value
        elif category == 'WSD':  # 풍속
            weather_info[key]['풍속(m/s)'] = value
    
    # 딕셔너리를 DataFrame으로 변환
    df = pd.DataFrame(list(weather_info.values()))
    
    # 정렬 (날짜, 시간 순)
    if len(df) > 0:
        df = df.sort_values(by=['날짜', '시간'])
    
    return df

###############################################
# 미세먼지 정보 관련 함수
###############################################

def get_air_quality_data(sido_name, station_name=None):
    """
    에어코리아 API를 통해 미세먼지 정보를 가져옵니다.
    
    Args:
        sido_name (str): 시도 이름(서울, 부산 등)
        station_name (str, optional): 측정소 이름
    
    Returns:
        dict: 미세먼지 정보 데이터
    """
    # URL 인코딩된 서비스 키 준비
    encoded_service_key = urllib.parse.quote_plus(WEATHER_API_KEY)
    
    # API 요청에 필요한 파라미터 설정
    url = "http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getCtprvnRltmMesureDnsty"
    
    params = {
        'serviceKey': WEATHER_API_KEY,
        'returnType': 'json',
        'numOfRows': '100',
        'pageNo': '1',
        'ver': '1.0',
        'sidoName': sido_name
    }
    
    try:
        # API 요청
        response = requests.get(url, params=params)
        
        # 응답 확인
        if response.status_code == 200:
            data = response.json()
            
            # 응답 결과 확인
            result_code = data.get('response', {}).get('header', {}).get('resultCode')
            
            if result_code == '00':  # 정상 응답
                items = data.get('response', {}).get('body', {}).get('items', [])
                total_count = data.get('response', {}).get('body', {}).get('totalCount', 0)
                print(f"미세먼지 정보: 총 {total_count}개의 측정소 정보를 가져왔습니다.")
                
                # 측정소 필터링
                if station_name and items:
                    filtered_items = [item for item in items if station_name in item.get('stationName', '')]
                    if filtered_items:
                        data['response']['body']['items'] = filtered_items
                        print(f"'{station_name}' 관련 측정소 {len(filtered_items)}개 필터링 완료")
                    else:
                        print(f"'{station_name}'에 해당하는 측정소가 없습니다. 전체 결과를 반환합니다.")
                
                return data
            else:
                result_msg = data.get('response', {}).get('header', {}).get('resultMsg', '알 수 없는 오류')
                print(f"미세먼지 API 응답 오류: {result_msg}")
                return None
        else:
            print(f"미세먼지 API HTTP 오류: {response.status_code}")
            return None
            
    except Exception as e:
        print(f"미세먼지 API 예외 발생: {e}")
        return None

def parse_air_quality_data(data):
    """
    미세먼지 데이터를 분석하여 보기 쉽게 정리합니다.
    
    Args:
        data (dict): API 응답 데이터
    
    Returns:
        DataFrame: 정리된 미세먼지 정보
    """
    if data is None:
        return None
    
    items = data.get('response', {}).get('body', {}).get('items', [])
    
    if not items:
        print("미세먼지 정보가 없습니다.")
        return None
    
    # 미세먼지 농도에 따른 등급 계산 함수
    def get_pm10_grade(value):
        try:
            value = float(value)
            if value <= 30:
                return '좋음'
            elif value <= 80:
                return '보통'
            elif value <= 150:
                return '나쁨'
            else:
                return '매우나쁨'
        except (ValueError, TypeError):
            return '정보없음'
    
    def get_pm25_grade(value):
        try:
            value = float(value)
            if value <= 15:
                return '좋음'
            elif value <= 50:
                return '보통'
            elif value <= 100:
                return '나쁨'
            else:
                return '매우나쁨'
        except (ValueError, TypeError):
            return '정보없음'
    
    # 기존 API 응답의 등급 정의 (다른 항목에 사용)
    grade_mapping = {
        '1': '좋음',
        '2': '보통',
        '3': '나쁨',
        '4': '매우나쁨',
        None: '정보없음'
    }
    
    # 필요한 데이터만 추출
    air_quality_data = []
    for item in items:
        # 측정 일시 변환
        data_time = item.get('dataTime', '')
        
        # 각 항목별 값 가져오기
        pm10_value = item.get('pm10Value', '-')
        pm25_value = item.get('pm25Value', '-')
        
        # 수치 기반으로 등급 계산
        pm10_grade = get_pm10_grade(pm10_value)
        pm25_grade = get_pm25_grade(pm25_value)
        
        # 다른 항목은 기존 방식대로 처리
        o3_value = item.get('o3Value', '-')
        o3_grade = grade_mapping.get(item.get('o3Grade'), '정보없음')
        
        no2_value = item.get('no2Value', '-')
        no2_grade = grade_mapping.get(item.get('no2Grade'), '정보없음')
        
        co_value = item.get('coValue', '-')
        co_grade = grade_mapping.get(item.get('coGrade'), '정보없음')
        
        so2_value = item.get('so2Value', '-')
        so2_grade = grade_mapping.get(item.get('so2Grade'), '정보없음')
        
        air_quality_data.append({
            '측정소명': item.get('stationName', ''),
            '시도': item.get('sidoName', ''),
            '측정일시': data_time,
            'PM10(㎍/㎥)': pm10_value,
            'PM10등급': pm10_grade,
            'PM2.5(㎍/㎥)': pm25_value,
            'PM2.5등급': pm25_grade,
            '오존(ppm)': o3_value,
            '오존등급': o3_grade,
            '이산화질소(ppm)': no2_value,
            '이산화질소등급': no2_grade,
            '일산화탄소(ppm)': co_value,
            '일산화탄소등급': co_grade,
            '아황산가스(ppm)': so2_value,
            '아황산가스등급': so2_grade
        })
    
    # 데이터프레임으로 변환
    df = pd.DataFrame(air_quality_data)

    return df

###############################################
# 데이터 파일 처리 함수
###############################################

def format_combined_data(weather_df, air_df, region, district):
    """
    날씨와 미세먼지 데이터를 하나로 통합하여 출력 형식으로 변환합니다.
    """
    now = datetime.now()
    current_time = now.strftime("%Y년 %m월 %d일 %H시 %M분")
    
    result = f"날씨정보 {region} {district} - {current_time} 기준\n"
    
    # 날씨 정보 추가
    if weather_df is not None and len(weather_df) > 0:
        try:
            current_hour = now.hour
            weather_df['시간_int'] = weather_df['시간'].apply(lambda x: int(x[:2]) if len(x) >= 2 else 0)
            closest_time_idx = (weather_df['시간_int'] - current_hour).abs().idxmin()
            current_weather = weather_df.loc[closest_time_idx]
            
            # 형식을 일관되게 유지 (모든 정보를 한 줄에 표시)
            result += f"기온 = {current_weather.get('기온(°C)', '-')}°C, "
            result += f"하늘 = {current_weather.get('하늘상태', '-')}, "
            result += f"강수확률 = {current_weather.get('강수확률(%)', '-')}%, "
            result += f"습도 = {current_weather.get('습도(%)', '-')}%, "
            result += f"풍속 = {current_weather.get('풍속(m/s)', '-')}m/s\n"
        except Exception as e:
            print(f"날씨 정보 처리 오류: {e}")
            result += "날씨 정보 처리 중 오류 발생\n"
    else:
        result += "날씨 정보 없음\n"
    
    # 미세먼지 정보 추가
    if air_df is not None and len(air_df) > 0:
        try:
            air_info = air_df.iloc[0]
            result += f"미세먼지(PM10) = {air_info.get('PM10등급', '-')}, "
            result += f"초미세먼지(PM2.5) = {air_info.get('PM2.5등급', '-')}"
        except Exception as e:
            print(f"미세먼지 정보 처리 오류: {e}")
            result += "미세먼지 정보 처리 중 오류 발생"
    else:
        result += "미세먼지 정보 없음"
    
    return result

def save_data_to_file(combined_data):
    """
    통합된 날씨와 미세먼지 정보를 파일로 저장합니다.
    
    Args:
        combined_data (str): 통합된 정보 문자열
        
    Returns:
        bool: 성공 여부
    """
    try:
        # 날씨 정보 파일 저장
        with open(WEATHER_FILE, 'w', encoding='utf-8') as f:
            f.write(combined_data)
        
        return True
    except Exception as e:
        print(f"파일 저장 오류: {e}")
        return False

###############################################
# 메인 함수
###############################################

def update_weather_data(location_name, direct_send=False):
    """
    특정 지역의 날씨와 미세먼지 정보를 가져와 파일로 저장하거나 직접 서버로 전송하는 함수
    
    Args:
        location_name (str): 지역명 또는 행정구역명
        direct_send (bool): 서버로 직접 전송 여부
        
    Returns:
        str: 통합된 날씨/미세먼지 정보 문자열
    """
    # 지역 정보 가져오기
    result, location_info = find_location_grid(location_name)
    
    if not isinstance(result, tuple) or location_info is None:
        # 지역 정보를 찾지 못한 경우
        print(f"'{location_name}' 지역을 찾을 수 없습니다. 기본 지역(서울 중구)을 사용합니다.")
        result, location_info = find_location_grid("서울 중구")
    
    nx, ny = result
    region = location_info['지역']
    district = location_info['행정구역']
    
    print(f"'{region} {district}' 지역의 날씨 및 미세먼지 정보를 조회합니다...")
    
    # 날씨 정보 조회
    weather_data = get_weather_data(nx, ny)
    weather_df = parse_weather_data(weather_data) if weather_data else None
    
    # 미세먼지 정보 조회
    air_data = get_air_quality_data(region)
    air_df = parse_air_quality_data(air_data) if air_data else None
    
    # 정보를 통합 형식으로 변환
    combined_data = format_combined_data(weather_df, air_df, region, district)
    
    # 파일로 저장
    if save_data_to_file(combined_data):
        print(f"통합 날씨 및 미세먼지 정보를 {WEATHER_FILE} 파일로 저장했습니다.")
    else:
        print("파일 저장에 실패했습니다.")
    
    # 직접 서버 전송 모드
    if direct_send:
        client_socket = connect_to_server()
        if client_socket:
            if send_to_server(client_socket, combined_data, 'USR_AI'):
                print("서버에 날씨 정보를 직접 전송했습니다.")
            else:
                print("서버 전송에 실패했습니다.")
            client_socket.close()
    
    return combined_data

def main():
    """
    날씨 정보 파일 업데이트 및 서버 전송 메인 함수
    AI 요청에 응답하는 기능 추가
    """
    import argparse
    import signal
    import sys
    
    global SERVER_IP, SERVER_PORT, DEVICE_NAME
    
    running = True
    client_socket = None
    
    def signal_handler(sig, frame):
        """시그널 핸들러: Ctrl+C로 인한 종료 처리"""
        nonlocal running, client_socket
        print("\n프로그램을 종료합니다.")
        running = False
        if client_socket:
            client_socket.close()
        sys.exit(0)
    
    # Ctrl+C 시그널 핸들러 등록
    signal.signal(signal.SIGINT, signal_handler)
    
    # 명령행 인자 설정
    parser = argparse.ArgumentParser(description='날씨 및 미세먼지 정보를 수집하여 파일로 저장하거나 서버로 전송하는 프로그램')
    parser.add_argument('-l', '--location', type=str, default=DEFAULT_LOCATION,
                        help=f'정보를 조회할 지역명 (기본값: {DEFAULT_LOCATION})')
    parser.add_argument('-i', '--interval', type=int, default=UPDATE_INTERVAL // 60,
                        help=f'업데이트 간격(분) (기본값: {UPDATE_INTERVAL // 60}분)')
    parser.add_argument('-o', '--once', action='store_true',
                        help='한 번만 실행하고 종료 (기본: 연속 실행)')
    parser.add_argument('-d', '--direct', action='store_true',
                        help='서버에 직접 전송 (기본: 파일로만 저장)')
    parser.add_argument('-s', '--server', type=str, default=SERVER_IP,
                        help=f'서버 IP 주소 (기본값: {SERVER_IP})')
    parser.add_argument('-p', '--port', type=int, default=SERVER_PORT,
                        help=f'서버 포트 번호 (기본값: {SERVER_PORT})')
    parser.add_argument('-n', '--name', type=str, default=DEVICE_NAME,
                        help=f'장치 이름 (기본값: {DEVICE_NAME})')
    parser.add_argument('-a', '--ai-mode', action='store_true', 
                        help='AI 응답 모드 활성화 (요청에 즉시 응답)')
    
    args = parser.parse_args()
    
    # 인자 처리
    location = args.location
    interval_sec = args.interval * 60
    once = args.once
    direct_send = args.direct
    ai_mode = args.ai_mode
    
    # 서버 설정 업데이트
    SERVER_IP = args.server
    SERVER_PORT = args.port
    DEVICE_NAME = args.name
    
    print(f"=== 날씨 & 미세먼지 정보 통합 업데이트 프로그램 ===")
    print(f"위치: {location}")
    
    if direct_send or ai_mode:
        print(f"서버 연결: {SERVER_IP}:{SERVER_PORT} (장치: {DEVICE_NAME})")
        
        # 초기 서버 연결
        client_socket = connect_to_server()
        if client_socket:
            print("서버 연결 성공!")
            if ai_mode:
                print("AI 응답 모드가 활성화되었습니다. 날씨 요청을 감시합니다.")
        else:
            print("서버 연결 실패! 파일 저장 모드로만 계속 진행합니다.")
            direct_send = False
            ai_mode = False
    
    if once:
        # 한 번만 실행하는 옵션이 활성화된 경우에만 초기 날씨 정보 수집
        combined_data = update_weather_data(location, direct_send)
        print("\n=== 수집된 날씨 및 미세먼지 정보 ===")
        print(combined_data)
    else:
        # 연속 실행 모드
        print(f"업데이트 간격: {args.interval}분")
        print("프로그램을 종료하려면 Ctrl+C를 누르세요.")
        print("'날씨' 요청을 기다리는 중... 요청이 오면 정보를 수집합니다.")
        
        last_update_time = 0
        connection_check_time = 0
        
        while running:
            current_time = time.time()
            
            # 연결 상태 확인 (60초마다)
            if ai_mode and (current_time - connection_check_time >= 60):
                connection_check_time = current_time
                
                # 소켓이 닫혔거나 없는 경우 재연결 시도
                if client_socket is None or client_socket.fileno() == -1:
                    print("서버 연결이 끊어졌습니다. 재연결을 시도합니다...")
                    client_socket = reconnect_to_server()
                    if client_socket:
                        print("서버 재연결 성공! 날씨 요청 감시를 재개합니다.")
                    else:
                        print("서버 재연결 실패! 60초 후에 다시 시도합니다.")
            
            # AI 모드에서 메시지 모니터링
            if ai_mode and client_socket and client_socket.fileno() != -1:
                try:
                    if not monitor_messages(client_socket):
                        # 모니터링 중 오류 발생하면 소켓을 닫고 재연결 준비
                        print("메시지 모니터링 중 오류가 발생했습니다. 연결을 재설정합니다...")
                        client_socket.close()
                        client_socket = reconnect_to_server()
                except Exception as e:
                    print(f"메시지 처리 중 예외 발생: {e}")
                    try:
                        client_socket.close()
                    except:
                        pass
                    client_socket = reconnect_to_server()
            
            # CPU 사용량 감소를 위한 짧은 sleep
            time.sleep(0.1)
        
        # 종료 처리
        if client_socket:
            client_socket.close()

if __name__ == "__main__":
    main()