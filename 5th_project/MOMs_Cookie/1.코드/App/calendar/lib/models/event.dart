class Event {
  final String id;
  final String title;
  final String description;
  final DateTime date;
  final bool isAllDay;
  final DateTime? startTime;
  final DateTime? endTime;
  final int priority; // 1: 낮음, 2: 중간, 3: 높음
  final bool synced; // 서버와 동기화 여부
  
  // 반복 관련 필드
  final bool isRepeating;
  final String repeatType; // 'none', 'weekly', 'monthly', 'yearly'

  const Event({
    required this.id,
    required this.title,
    required this.description,
    required this.date,
    this.isAllDay = false,
    this.startTime,
    this.endTime,
    required this.priority,
    this.synced = false,
    this.isRepeating = false,
    this.repeatType = 'none',
  });

  // JSON 직렬화
  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'title': title,
      'description': description,
      'date': date.toIso8601String(),
      'isAllDay': isAllDay ? 1 : 0,
      'startTime': startTime?.toIso8601String(),
      'endTime': endTime?.toIso8601String(),
      'priority': priority,
      'synced': synced ? 1 : 0,
      'isRepeating': isRepeating ? 1 : 0,
      'repeatType': repeatType,
    };
  }

  // JSON에서 객체 생성
  factory Event.fromJson(Map<String, dynamic> json) {
    return Event(
      id: json['id'],
      title: json['title'],
      description: json['description'],
      date: DateTime.parse(json['date']),
      isAllDay: json['isAllDay'] == 1,
      startTime:
          json['startTime'] != null ? DateTime.parse(json['startTime']) : null,
      endTime: json['endTime'] != null ? DateTime.parse(json['endTime']) : null,
      priority: json['priority'],
      synced: json['synced'] == 1,
      isRepeating: json['isRepeating'] == 1,
      repeatType: json['repeatType'] ?? 'none',
    );
  }

  // 객체 복사 (속성 변경 시 사용)
  Event copyWith({
    String? id,
    String? title,
    String? description,
    DateTime? date,
    bool? isAllDay,
    DateTime? startTime,
    DateTime? endTime,
    int? priority,
    bool? synced,
    bool? isRepeating,
    String? repeatType,
  }) {
    return Event(
      id: id ?? this.id,
      title: title ?? this.title,
      description: description ?? this.description,
      date: date ?? this.date,
      isAllDay: isAllDay ?? this.isAllDay,
      startTime: startTime ?? this.startTime,
      endTime: endTime ?? this.endTime,
      priority: priority ?? this.priority,
      synced: synced ?? this.synced,
      isRepeating: isRepeating ?? this.isRepeating,
      repeatType: repeatType ?? this.repeatType,
    );
  }

  // 서버 메시지 형식으로 변환 (반복 정보 포함)
  String toServerMessage() {
    // 형식: [수신자]:명령어:제목:날짜:중요도:반복여부:반복타입
    return "Weather:SCHEDULE_FILE:$title:${date.year}/${date.month}/${date.day}:$priority:${isRepeating ? 1 : 0}:$repeatType";
  }
}