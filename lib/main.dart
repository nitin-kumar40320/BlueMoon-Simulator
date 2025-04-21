import 'package:flutter/material.dart';
import 'package:riscv_simulator/editor_tab.dart';
import 'package:riscv_simulator/pipeline_visualise.dart';
import 'package:riscv_simulator/simulator_tab.dart';
import 'package:riscv_simulator/theme.dart';

void main() {
  Future.delayed(const Duration(seconds: 2), () {
    runApp(const RiscVSimulatorApp());
  });
}

class RiscVSimulatorApp extends StatelessWidget {
  const RiscVSimulatorApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'RISC-V Simulator',
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: ThemeMode.light,
      debugShowCheckedModeBanner: false,
      home: const SimulatorHomePage(),
    );
  }
}

class SimulatorHomePage extends StatefulWidget {
  const SimulatorHomePage({super.key});

  @override
  State<SimulatorHomePage> createState() => _SimulatorHomePageState();
}

class _SimulatorHomePageState extends State<SimulatorHomePage> {
  int selectedIndex = 0;
  String codeContent = '';

  void updateCodeContent(String newContent) {
    setState(() {
      codeContent = newContent;
    });
  }

  void selectTab(int index) {
    setState(() {
      selectedIndex = index;
    });
  }

  Widget _buildSidebarItem(
      {required IconData icon, required String label, required int index}) {
    final bool isSelected = selectedIndex == index;
    return InkWell(
      onTap: () => selectTab(index),
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 16.0),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon,
                color: isSelected ? Colors.white : Colors.grey, size: 40),
            const SizedBox(height: 4),
            Text(
              label,
              style: TextStyle(
                color: isSelected ? Colors.white : Colors.grey,
                fontSize: 12,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildContent() {
    // Switch between Editor and Simulator based on the selectedIndex.
    if (selectedIndex == 0) {
      return EditorTab(
        codeContent: codeContent,
        onCodeChanged: updateCodeContent,
      );
    } 
    else if (selectedIndex == 2)
    {
      return PipelineBlockDiagram(codeContent: codeContent);
    }
    else {
      return SimulatorTab(
        codeContent: codeContent,
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Colors.white,
        surfaceTintColor: Colors.white,
        centerTitle: true,
        title: Row(
          children: [
            Text('BlueM',
                style: TextStyle(
                    fontWeight: FontWeight.bold, color: Colors.blue[900])),
            Icon(Icons.toll_rounded, color: Colors.blue[900]),
            Text('n Simulator',
                style: TextStyle(
                    fontWeight: FontWeight.bold, color: Colors.blue[900])),
          ],
        ),
        elevation: 0,
        actions: [
          IconButton(
            icon: Icon(Icons.help_outline, color: Colors.blue[900]),
            onPressed: () {
              // Show help dialog
              showDialog(
                context: context,
                builder: (context) => AlertDialog(
                  backgroundColor: Colors.blue[900],
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                  title: const Text(
                    'RISC-V Simulator Help',
                    style: TextStyle(color: Colors.white),
                  ),
                  content: SingleChildScrollView(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      mainAxisSize: MainAxisSize.min,
                      children: const [
                        Text(
                          'Editor Tab:',
                          style: TextStyle(
                              fontWeight: FontWeight.bold, color: Colors.white),
                        ),
                        Text(
                            '• Enter RISC-V machine code in hexadecimal format',
                            style: TextStyle(color: Colors.white)),
                        Text('• Each instruction should be on a new line',
                            style: TextStyle(color: Colors.white)),
                        Text('• Comments can be added after # symbol',
                            style: TextStyle(color: Colors.white)),
                        SizedBox(height: 12),
                        Text(
                          'Simulator Tab:',
                          style: TextStyle(
                              fontWeight: FontWeight.bold, color: Colors.white),
                        ),
                        Text('• Run: Execute the entire program',
                            style: TextStyle(color: Colors.white)),
                        Text('• Step: Execute one instruction at a time',
                            style: TextStyle(color: Colors.white)),
                        Text('• Reset: Clear all registers and memory',
                            style: TextStyle(color: Colors.white)),
                      ],
                    ),
                  ),
                  actions: [
                    ElevatedButton(
                      onPressed: () => Navigator.of(context).pop(),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.white,
                        elevation: 3
                      ),
                      child: Text(
                        'Close',
                        style: TextStyle(color: Colors.blue[900]),
                      ),
                    ),
                  ],
                ),
              );
            },
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: Row(
        children: [
          // Permanent vertical sidebar
          Container(
            width: 85, // fixed width
            color: Colors.blue[900],
            child: Column(
              mainAxisAlignment: MainAxisAlignment.start,
              children: [
                const SizedBox(height: 12),
                _buildSidebarItem(icon: Icons.code, label: 'Editor', index: 0),
                _buildSidebarItem(icon: Icons.memory, label: 'Simulator', index: 1),
                _buildSidebarItem(icon: Icons.grid_view_rounded, label: 'Visualise', index: 2),
              ],
            ),
          ),
          // Main content area
          Expanded(
            child: Container(
              decoration: BoxDecoration(color: Colors.grey[600]),
              child: _buildContent(),
            ),
          ),
        ],
      ),
    );
  }
}