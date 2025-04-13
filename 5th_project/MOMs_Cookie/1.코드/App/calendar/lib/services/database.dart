import 'package:sqflite/sqflite.dart';
import 'package:path/path.dart';
import '../models/event.dart';

class DatabaseService {
  static final DatabaseService _instance = DatabaseService._internal();
  
  factory DatabaseService() => _instance;
  
  DatabaseService._internal();

  static Database? _database;

  Future<Database> get database async {
    if (_database != null) return _database!;
    
    _database = await _initDB();
    return _database!;
  }

  Future<Database> _initDB() async {
    final dbPath = await getDatabasesPath();
    final path = join(dbPath, 'events.db');

    return await openDatabase(
      path,
      version: 2, // 버전 업데이트
      onCreate: _createDB,
      onUpgrade: _upgradeDB,
    );
  }

  Future<void> _createDB(Database db, int version) async {
    await db.execute('''
      CREATE TABLE events(
        id TEXT PRIMARY KEY,
        title TEXT NOT NULL,
        description TEXT,
        date TEXT NOT NULL,
        isAllDay INTEGER NOT NULL,
        startTime TEXT,
        endTime TEXT,
        priority INTEGER NOT NULL,
        synced INTEGER NOT NULL DEFAULT 0,
        isRepeating INTEGER NOT NULL DEFAULT 0,
        repeatType TEXT NOT NULL DEFAULT 'none'
      )
    ''');
  }

  // 데이터베이스 업그레이드 처리
  Future<void> _upgradeDB(Database db, int oldVersion, int newVersion) async {
    if (oldVersion < 2) {
      // 버전 1에서 2로 업그레이드: 반복 관련 필드 추가
      await db.execute("ALTER TABLE events ADD COLUMN isRepeating INTEGER NOT NULL DEFAULT 0");
      await db.execute("ALTER TABLE events ADD COLUMN repeatType TEXT NOT NULL DEFAULT 'none'");
    }
  }

  // 이벤트 추가
  Future<int> insertEvent(Event event) async {
    final db = await database;
    return await db.insert(
      'events',
      event.toJson(),
      conflictAlgorithm: ConflictAlgorithm.replace,
    );
  }

  // 이벤트 업데이트
  Future<int> updateEvent(Event event) async {
    final db = await database;
    return await db.update(
      'events',
      event.toJson(),
      where: 'id = ?',
      whereArgs: [event.id],
    );
  }

  // 이벤트 삭제
  Future<int> deleteEvent(String id) async {
    final db = await database;
    return await db.delete(
      'events',
      where: 'id = ?',
      whereArgs: [id],
    );
  }

  // 모든 이벤트 조회
  Future<List<Event>> getEvents() async {
    final db = await database;
    final maps = await db.query('events');

    return List.generate(maps.length, (i) {
      return Event.fromJson(maps[i]);
    });
  }

  // 특정 날짜의 이벤트 조회 (반복 이벤트 포함)
  Future<List<Event>> getEventsForDay(DateTime day) async {
    final db = await database;
    final dateString = day.toIso8601String().split('T')[0];
    List<Event> eventList = [];
    
    // 1. 해당 날짜에 직접 등록된 이벤트 가져오기
    final directEvents = await db.query(
      'events',
      where: "date LIKE ?",
      whereArgs: ['$dateString%'],
    );
    
    for (var event in directEvents) {
      eventList.add(Event.fromJson(event));
    }
    
    // 2. 반복 이벤트 확인하기
    final allEvents = await db.query(
      'events',
      where: "isRepeating = ?",
      whereArgs: [1],
    );
    
    for (var eventMap in allEvents) {
      final event = Event.fromJson(eventMap);
      final eventDate = event.date;
      
      // 매주 반복 이벤트 체크
      if (event.repeatType == 'weekly' && 
          eventDate.weekday == day.weekday && 
          (eventDate.isBefore(day) || isSameDate(eventDate, day))) {
        // 이벤트 날짜 조정하여 추가
        final adjustedEvent = event.copyWith(
          date: DateTime(day.year, day.month, day.day, 
                         eventDate.hour, eventDate.minute),
        );
        eventList.add(adjustedEvent);
      }
      
      // 매월 반복 이벤트 체크
      else if (event.repeatType == 'monthly' && 
               eventDate.day == day.day && 
               (eventDate.isBefore(day) || isSameDate(eventDate, day))) {
        final adjustedEvent = event.copyWith(
          date: DateTime(day.year, day.month, day.day, 
                         eventDate.hour, eventDate.minute),
        );
        eventList.add(adjustedEvent);
      }
      
      // 매년 반복 이벤트 체크
      else if (event.repeatType == 'yearly' && 
               eventDate.month == day.month && 
               eventDate.day == day.day && 
               (eventDate.isBefore(day) || isSameDate(eventDate, day))) {
        final adjustedEvent = event.copyWith(
          date: DateTime(day.year, day.month, day.day, 
                         eventDate.hour, eventDate.minute),
        );
        eventList.add(adjustedEvent);
      }
    }
    
    return eventList;
  }

  // 같은 날짜인지 확인하는 헬퍼 메서드
  bool isSameDate(DateTime date1, DateTime date2) {
    return date1.year == date2.year && 
           date1.month == date2.month && 
           date1.day == date2.day;
  }

  // 동기화 상태 업데이트
  Future<int> updateSyncStatus(String id, bool synced) async {
    final db = await database;
    return await db.update(
      'events',
      {'synced': synced ? 1 : 0},
      where: 'id = ?',
      whereArgs: [id],
    );
  }

  // 동기화되지 않은 이벤트 조회
  Future<List<Event>> getUnsyncedEvents() async {
    final db = await database;
    final maps = await db.query(
      'events',
      where: 'synced = ?',
      whereArgs: [0],
    );

    return List.generate(maps.length, (i) {
      return Event.fromJson(maps[i]);
    });
  }
}