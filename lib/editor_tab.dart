import 'package:flutter/material.dart';
import 'package:flutter_code_editor/flutter_code_editor.dart';
import 'package:another_flushbar/flushbar.dart';
import 'package:riscv_simulator/wasm_interop.dart';

class EditorTab extends StatefulWidget {
  final String codeContent;
  final Function(String) onCodeChanged;

  const EditorTab({
    super.key,
    required this.codeContent,
    required this.onCodeChanged,
  });

  @override
  State<EditorTab> createState() => _EditorTabState();
}

class _EditorTabState extends State<EditorTab> {
  late CodeController _codeController;
  late RiscVSimulator _simulator;
  bool _isSimulatorInitialized = false;

  @override
  void initState() {
    super.initState();
    _codeController = CodeController(
      text: widget.codeContent,
    );

    _codeController.addListener(() {
      widget.onCodeChanged(_codeController.text);
    });

    // Initialize the simulator when the editor is created
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _initializeSimulator();
    });
  }

  Future<void> _initializeSimulator() async {
    try {
      // Await the async factory constructor
      _simulator = await RiscVSimulator.create();
      // Since the simulator is now ready, initialize it.
      _simulator.init();
      setState(() {
        _isSimulatorInitialized = true;
      });
    } catch (e) {
      print('Error initializing simulator: $e');
      _showErrorMessage('Failed to initialize simulator: $e');
    }
  }

  void _saveCodeToSimulator() {
    if (!_isSimulatorInitialized) {
      _showErrorMessage('Simulator not initialized');
      return;
    }

    try {
      _simulator.reset();
      String asmCode = _simulator.assemble(_codeController.text);
      print(asmCode);
      _simulator.loadCode(asmCode);

      // Show success message
      _showSuccessMessage('Code saved and loaded into simulator');
    } catch (e) {
      _showErrorMessage('Error saving code to simulator: $e');
    }
  }

  void _clearEditor() {
    setState(() {
      _codeController.text = '';
    });

    // Also reset the simulator
    if (_isSimulatorInitialized) {
      _simulator.reset();
    }
  }

  void _showSuccessMessage(String message) {
    Flushbar(
      messageText: Text(
        message,
        textAlign: TextAlign.center,
        style: const TextStyle(color: Colors.white),
      ),
      duration: const Duration(seconds: 3),
      flushbarPosition: FlushbarPosition.TOP,
      flushbarStyle: FlushbarStyle.FLOATING,
      backgroundColor: Colors.blue[900]!,
      icon: const Icon(Icons.check_circle, color: Colors.white),
      margin: const EdgeInsets.all(8),
      borderRadius: BorderRadius.circular(10),
      maxWidth: 300,
    ).show(context);
  }

  void _showErrorMessage(String message) {
    Flushbar(
      messageText: Text(
        message,
        textAlign: TextAlign.center,
        style: const TextStyle(color: Colors.white),
      ),
      duration: const Duration(seconds: 3),
      flushbarPosition: FlushbarPosition.TOP,
      flushbarStyle: FlushbarStyle.FLOATING,
      backgroundColor: Colors.red,
      icon: const Icon(Icons.error, color: Colors.white),
      margin: const EdgeInsets.all(8),
      borderRadius: BorderRadius.circular(10),
      maxWidth: 300,
    ).show(context);
  }

  @override
  void didUpdateWidget(EditorTab oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.codeContent != widget.codeContent) {
      _codeController.text = widget.codeContent;
    }
  }

  @override
  void dispose() {
    _codeController.dispose();
    if (_isSimulatorInitialized) {
      _simulator.cleanup();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(12.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              ElevatedButton.icon(
                onPressed:
                    _isSimulatorInitialized ? _saveCodeToSimulator : null,
                icon: const Icon(Icons.save, color: Colors.white),
                label:
                    const Text('Save', style: TextStyle(color: Colors.white)),
                style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue[900],
                    elevation: 4,
                    shadowColor: Colors.black),
              ),
              const SizedBox(width: 12),
              ElevatedButton.icon(
                onPressed: _clearEditor,
                icon: Icon(Icons.delete, color: Colors.blue[900]),
                label: Text('Clear', style: TextStyle(color: Colors.blue[900])),
                style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.white,
                    elevation: 4,
                    shadowColor: Colors.black),
              ),
              const SizedBox(width: 12),
              ElevatedButton.icon(
                onPressed:
                    _isSimulatorInitialized ? () => _loadExampleCode() : null,
                icon: Icon(Icons.code, color: Colors.blue[900]),
                label: Text('Load Example',
                    style: TextStyle(color: Colors.blue[900])),
                style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.white,
                    elevation: 4,
                    shadowColor: Colors.black),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Expanded(
            child: Card(
              elevation: 8,
              color: Colors.grey[900],
              clipBehavior: Clip.antiAlias,
              child: SingleChildScrollView(
                child: Padding(
                  padding: const EdgeInsets.all(8.0),
                  child: CodeTheme(
                    data: CodeThemeData(),
                    child: CodeField(
                      controller: _codeController,
                      background: Colors.grey[900],
                      textStyle:
                          const TextStyle(fontSize: 14, fontFamily: 'SpaceMono'),
                    ),
                  ),
                ),
              ),
            ),
          ),
          if (!_isSimulatorInitialized)
            Padding(
              padding: const EdgeInsets.only(top: 8.0),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.warning_amber_rounded, color: Colors.amber),
                  const SizedBox(width: 8),
                  Text(
                    'WebAssembly simulator not initialized. Some features may be unavailable.',
                    style: TextStyle(color: Colors.amber[800]),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }

  void _loadExampleCode() {
    // Simple example RISC-V program
    const exampleCode = '''
# Simple RISC-V program example
# This program adds two numbers and stores the result

00500113 # addi x2, x0, 5    # Load 5 into x2
00A00193 # addi x3, x0, 10   # Load 10 into x3
003100B3 # add x1, x2, x3    # Add x2 and x3, store in x1
00102223 # sw x1, 4(x0)      # Store x1 at memory address 4
00002103 # lw x2, 0(x0)      # Load from memory address 0 into x2
00000073 # ecall             # End program
''';

    setState(() {
      _codeController.text = exampleCode;
    });

    _showSuccessMessage('Example code loaded');
  }
}
