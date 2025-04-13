import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:uuid/uuid.dart';
import 'package:intl/intl.dart';
import '../models/event.dart';
import '../services/event_provider.dart';
import '../widgets/priority_selector.dart';
import '../widgets/repeat_selector.dart'; // 반복 선택 위젯 추가

class EventScreen extends StatefulWidget {
  final Event? event;
  final DateTime selectedDate;

  const EventScreen({super.key, this.event, required this.selectedDate});

  @override
  State<EventScreen> createState() => _EventScreenState();
}

class _EventScreenState extends State<EventScreen> {
  final _formKey = GlobalKey<FormState>();
  late TextEditingController _titleController;
  late TextEditingController _descriptionController;
  late DateTime _selectedDate;
  late bool _isAllDay;
  TimeOfDay? _startTime;
  TimeOfDay? _endTime;
  late int _priority;

  // 반복 관련 상태 추가
  late bool _isRepeating;
  late String _repeatType;

  @override
  void initState() {
    super.initState();

    final event = widget.event;

    // 기존 이벤트 수정인 경우 값 설정
    if (event != null) {
      _titleController = TextEditingController(text: event.title);
      _descriptionController = TextEditingController(text: event.description);
      _selectedDate = event.date;
      _isAllDay = event.isAllDay;
      _startTime =
          event.startTime != null
              ? TimeOfDay(
                hour: event.startTime!.hour,
                minute: event.startTime!.minute,
              )
              : null;
      _endTime =
          event.endTime != null
              ? TimeOfDay(
                hour: event.endTime!.hour,
                minute: event.endTime!.minute,
              )
              : null;
      _priority = event.priority;

      // 반복 설정 불러오기 - 기본 false 및 'weekly'로 설정
      _isRepeating = false;
      _repeatType = 'weekly';
    }
    // 새 이벤트 추가인 경우 기본값 설정
    else {
      _titleController = TextEditingController();
      _descriptionController = TextEditingController();
      _selectedDate = widget.selectedDate;
      _isAllDay = false;
      _startTime = TimeOfDay.now();
      final now = TimeOfDay.now();
      _endTime = TimeOfDay(hour: now.hour + 1, minute: now.minute);
      _priority = 2; // 기본 중요도는 중간

      // 반복 기본값
      _isRepeating = false;
      _repeatType = 'weekly'; // 기본 반복 타입
    }
  }

  @override
  void dispose() {
    _titleController.dispose();
    _descriptionController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.event == null ? '새 일정' : '일정 수정'),
        actions: [
          IconButton(icon: const Icon(Icons.check), onPressed: _saveEvent),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16.0),
        child: Form(
          key: _formKey,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // 제목 입력
              TextFormField(
                controller: _titleController,
                decoration: const InputDecoration(
                  labelText: '제목',
                  border: OutlineInputBorder(),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return '제목을 입력해주세요';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),

              // 설명 입력
              TextFormField(
                controller: _descriptionController,
                decoration: const InputDecoration(
                  labelText: '설명',
                  border: OutlineInputBorder(),
                ),
                maxLines: 3,
              ),
              const SizedBox(height: 16),

              // 날짜 선택
              ListTile(
                title: const Text('날짜'),
                subtitle: Text(
                  DateFormat('yyyy년 MM월 dd일').format(_selectedDate),
                ),
                leading: const Icon(Icons.calendar_today),
                onTap: () => _selectDate(context),
              ),

              // 종일 일정 스위치
              SwitchListTile(
                title: const Text('종일 일정'),
                value: _isAllDay,
                onChanged: (value) {
                  setState(() {
                    _isAllDay = value;
                  });
                },
              ),

              // 시작 및 종료 시간 (종일 일정이 아닌 경우)
              if (!_isAllDay) ...[
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: ListTile(
                        title: const Text('시작 시간'),
                        subtitle: Text(
                          _startTime != null
                              ? '${_startTime!.hour.toString().padLeft(2, '0')}:${_startTime!.minute.toString().padLeft(2, '0')}'
                              : '선택 안함',
                        ),
                        leading: const Icon(Icons.access_time),
                        onTap: () => _selectStartTime(context),
                      ),
                    ),
                    Expanded(
                      child: ListTile(
                        title: const Text('종료 시간'),
                        subtitle: Text(
                          _endTime != null
                              ? '${_endTime!.hour.toString().padLeft(2, '0')}:${_endTime!.minute.toString().padLeft(2, '0')}'
                              : '선택 안함',
                        ),
                        leading: const Icon(Icons.access_time),
                        onTap: () => _selectEndTime(context),
                      ),
                    ),
                  ],
                ),
              ],

              const SizedBox(height: 16),

              // 반복 설정 추가
              RepeatSelector(
                isRepeating: _isRepeating,
                repeatType: _repeatType,
                onRepeatingChanged: (value) {
                  setState(() {
                    _isRepeating = value;
                  });
                },
                onRepeatTypeChanged: (type) {
                  setState(() {
                    _repeatType = type;
                  });
                },
              ),

              const SizedBox(height: 16),

              // 중요도 선택
              PrioritySelector(
                selectedPriority: _priority,
                onPriorityChanged: (priority) {
                  setState(() {
                    _priority = priority;
                  });
                },
              ),
            ],
          ),
        ),
      ),
    );
  }

  // 날짜 선택 메서드 - BuildContext 비동기 사용 오류 수정
  Future<void> _selectDate(BuildContext context) async {
    final BuildContext currentContext = context;
    final pickedDate = await showDatePicker(
      context: currentContext,
      initialDate: _selectedDate,
      firstDate: DateTime(2020),
      lastDate: DateTime(2030),
    );

    if (pickedDate != null && mounted) {
      setState(() {
        _selectedDate = pickedDate;
      });
    }
  }

  // 시작 시간 선택 메서드 - BuildContext 비동기 사용 오류 수정
  Future<void> _selectStartTime(BuildContext context) async {
    final BuildContext currentContext = context;
    final pickedTime = await showTimePicker(
      context: currentContext,
      initialTime: _startTime ?? TimeOfDay.now(),
    );

    if (pickedTime != null && mounted) {
      setState(() {
        _startTime = pickedTime;

        // 종료 시간이 시작 시간보다 이르면 자동 조정
        if (_endTime != null) {
          final startMinutes = pickedTime.hour * 60 + pickedTime.minute;
          final endMinutes = _endTime!.hour * 60 + _endTime!.minute;

          if (endMinutes <= startMinutes) {
            _endTime = TimeOfDay(
              hour: (startMinutes + 60) ~/ 60 % 24,
              minute: pickedTime.minute,
            );
          }
        }
      });
    }
  }

  // 종료 시간 선택 메서드 - BuildContext 비동기 사용 오류 수정
  Future<void> _selectEndTime(BuildContext context) async {
    final BuildContext currentContext = context;
    final pickedTime = await showTimePicker(
      context: currentContext,
      initialTime: _endTime ?? TimeOfDay.now(),
    );

    if (pickedTime != null && mounted) {
      setState(() {
        _endTime = pickedTime;
      });
    }
  }

  void _saveEvent() {
    if (_formKey.currentState!.validate()) {
      final provider = Provider.of<EventProvider>(context, listen: false);

      // 시작 및 종료 시간 계산
      DateTime? startDateTime;
      DateTime? endDateTime;

      if (!_isAllDay && _startTime != null) {
        startDateTime = DateTime(
          _selectedDate.year,
          _selectedDate.month,
          _selectedDate.day,
          _startTime!.hour,
          _startTime!.minute,
        );
      }

      if (!_isAllDay && _endTime != null) {
        endDateTime = DateTime(
          _selectedDate.year,
          _selectedDate.month,
          _selectedDate.day,
          _endTime!.hour,
          _endTime!.minute,
        );
      }

      // 이벤트 객체 생성 (반복 속성 추가)
      final event = Event(
        id: widget.event?.id ?? const Uuid().v4(),
        title: _titleController.text,
        description: _descriptionController.text,
        date: _selectedDate,
        isAllDay: _isAllDay,
        startTime: startDateTime,
        endTime: endDateTime,
        priority: _priority,
        synced: false,
        isRepeating: _isRepeating, // 반복 여부 설정
        repeatType: _repeatType, // 반복 타입 설정
      );

      // 추가 또는 수정
      if (widget.event == null) {
        provider.addEvent(event);
      } else {
        provider.updateEvent(widget.event!, event);
      }

      // 이전 화면으로 돌아가기
      Navigator.pop(context, true);
    }
  }
}