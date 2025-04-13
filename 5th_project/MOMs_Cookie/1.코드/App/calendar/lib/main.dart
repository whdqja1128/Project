import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'screens/main_screen.dart';
import 'services/event_provider.dart';
import 'services/socket_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  // 앱 시작 시 소켓 서비스 초기화 및 연결
  final socketService = SocketService();
  await socketService.loadSettings();
  socketService.connect();
  
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (context) => EventProvider(),
      child: MaterialApp(
        title: 'MOM\'s Cookie',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          primarySwatch: Colors.blue,
          visualDensity: VisualDensity.adaptivePlatformDensity,
          appBarTheme: const AppBarTheme(
            backgroundColor: Colors.blue,
            foregroundColor: Colors.white,
            elevation: 0,
          ),
        ),
        home: const MainScreen(),
      ),
    );
  }
}