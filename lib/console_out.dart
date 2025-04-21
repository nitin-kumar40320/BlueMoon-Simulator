import 'package:flutter/material.dart';

class ConsoleOutputWidget extends StatefulWidget {
  const ConsoleOutputWidget({super.key});

  @override
  State<ConsoleOutputWidget> createState() => ConsoleOutputWidgetState();
}

class ConsoleOutputWidgetState extends State<ConsoleOutputWidget> {
  String consoleOutput = '';
  final ScrollController _scrollController = ScrollController();

  // Appends new text and auto-scrolls to the bottom.
  void appendText(String text) {
    setState(() {
      consoleOutput += '$text\n';
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOut,
        );
      }
    });
  }

  // Clears the console.
  void clearConsole() {
    setState(() {
      consoleOutput = '';
    });
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 8,
      color: Colors.white,
      clipBehavior: Clip.antiAlias,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Container(
            padding: const EdgeInsets.all(12.0),
            color: Colors.grey.shade800,
            child: Row(
              children: [
                const Icon(Icons.terminal, color: Colors.white),
                const SizedBox(width: 8),
                const Text(
                  'Console Output',
                  style: TextStyle(
                    fontWeight: FontWeight.bold,
                    color: Colors.white,
                  ),
                ),
                const Spacer(),
                IconButton(
                  icon:
                      const Icon(Icons.clear_all, color: Colors.white, size: 20),
                  onPressed: () {
                    clearConsole();
                  },
                  tooltip: 'Clear console',
                  padding: EdgeInsets.zero,
                  constraints: const BoxConstraints(),
                ),
              ],
            ),
          ),
          Expanded(
            child: Container(
              color: Colors.black87,
              padding: const EdgeInsets.all(12.0),
              child: SingleChildScrollView(
                controller: _scrollController,
                child: Text(
                  consoleOutput,
                  style: const TextStyle(
                    color: Colors.lightGreenAccent,
                    fontFamily: 'SpaceMono',
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}