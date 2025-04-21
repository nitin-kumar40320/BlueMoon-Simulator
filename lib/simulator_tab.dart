import 'package:flutter/material.dart';
import 'package:another_flushbar/flushbar.dart';
import 'package:riscv_simulator/console_out.dart';
import 'package:riscv_simulator/wasm_interop.dart';

class SimulatorTab extends StatefulWidget {
  final String codeContent;

  const SimulatorTab({
    super.key,
    required this.codeContent,
  });

  @override
  State<SimulatorTab> createState() => _SimulatorTabState();
}

class _SimulatorTabState extends State<SimulatorTab> {
  bool isRunning = false;
  bool displayHex = true;
  bool displayBlockDiagram = false;
  int currentPc = 0;

  //Knob boolean variables
  bool pipelineEnable = true;
  bool dataForwardingEnable = false;
  bool showReg = false;
  bool showBuffers = false;
  bool showBP = false;


  final GlobalKey<ConsoleOutputWidgetState> _consoleKey =
      GlobalKey<ConsoleOutputWidgetState>();

  // WASM simulator instance
  late Simulator _simulator;
  bool _isSimulatorInitialized = false;
  String mcCode = "";

  // Memory navigation
  int memoryStartAddress = 0x00000000;
  final int memoryPageSize = 10;
  final int memoryMinAddress = 0x00000000;
  final int memoryMaxAddress = 0x7FFFFFFC;
  int memoryStartIndex = 0;
  final TextEditingController memoryJumpController = TextEditingController();
  Map<String, String> BTB = {};
  Map<String, bool> BHT = {};

  // Segment selection for memory view
  String currentSegment = 'Text';
  final List<String> segments = ['Text', 'Data', 'Stack'];

  // Registers data
  List<Map<String, dynamic>> registers = List.generate(
    32,
    (index) => {
      'name': index == 0 ? 'zero' : 'x$index',
      'value': '00000000',
    },
  );

  // Memory data
  Map<String, String> memoryMap = {};

  @override
  void initState() {
    super.initState();

    // Initialize the simulator when the tab is created
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _initializeSimulator();
    });
  }

  Future<void> _initializeSimulator() async {
    try {
      // Await the async factory method instead of calling the constructor directly.
      (!pipelineEnable)? _simulator = await RiscVSimulator.create():
          _simulator= await RiscVPipelinedSimulator.create();
      // Now that the module is loaded, initialize the simulator.
      _simulator.init();
      setState(() {
        _isSimulatorInitialized = true;
      });

      // Load the code if available.
      if (widget.codeContent.isNotEmpty) {
        _loadCode();
      }

      _updateRegisters();
      _updateMemory();
      _updateBP();
      _updatePC();
    } catch (e) {
      _consoleKey.currentState?.appendText('Error initializing simulator: $e');
    }
  }

  void _loadCode() {
    try {
      mcCode = _simulator.assemble(widget.codeContent);
      _simulator.loadCode(mcCode);
      _consoleKey.currentState?.appendText('Code loaded successfully');
    } catch (e) {
      _consoleKey.currentState?.appendText('Error loading code: $e');
    }
  }

  void _updateBP() {
    try {
      if (!_isSimulatorInitialized) return;

      final btbData = _simulator.getBP();
      print(btbData);
      final btbPairs = btbData.split(';');
      BTB.clear();
      BHT.clear();
      for (final pair in btbPairs) {
        if (pair.isEmpty) continue;

        final parts = pair.split(':');
        if (parts.length != 2) continue;

        final pc = parts[0];
        final btbEntry = parts[1].split(',');
        BTB[pc] = btbEntry[0];
        BHT[pc] = btbEntry[1] == 'true';
      }
      
      setState(() {});
    } catch (e) {
      _consoleKey.currentState?.appendText('Error updating BP: $e');
    }
  }

  void _updateRegisters() {
    try {
      if (!_isSimulatorInitialized) return;

      final regData = _simulator.showReg();
      final regPairs = regData.split(';');

      setState(() {
        for (final pair in regPairs) {
          if (pair.isEmpty) continue;

          final parts = pair.split(':');
          if (parts.length != 2) continue;

          final regName = parts[0];
          final regValue = parts[1];

          final regIndex = int.tryParse(regName.substring(1)) ?? 0;
          if (regIndex >= 0 && regIndex < registers.length) {
            registers[regIndex]['value'] = regValue;
          }
        }
      });
    } catch (e) {
      _consoleKey.currentState?.appendText('Error updating registers: $e');
    }
  }

  void _updateMemory() {
    try {
      if (!_isSimulatorInitialized) return;

      // Map segment names
      String segmentName;
      switch (currentSegment) {
        case 'Text':
          segmentName = 'text';
          break;
        case 'Data':
          segmentName = 'static';
          break;
        case 'Stack':
          segmentName = 'dynamic';
          break;
        default:
          segmentName = 'text';
      }

      final memData = _simulator.showMem(segmentName, memoryStartAddress, memoryPageSize * 4);
      final memPairs = memData.split(';');
      setState(() {
        memoryMap.clear();
        for (final pair in memPairs) {
          if (pair.isEmpty) continue;

          final parts = pair.split(':');
          if (parts.length != 2) continue;

          final addr = parts[0];
          final value = parts[1];

          memoryMap[addr] = value;
        }
      });
    } catch (e) {
      _consoleKey.currentState?.appendText('Error updating memory: $e');
    }
  }

  void _updatePC() {
    try {
      if (!_isSimulatorInitialized) return;

      final pc = _simulator.getPC();
      setState(() {
        currentPc = int.tryParse(pc, radix: 16) ?? 0;
      });
    } catch (e) {
      _consoleKey.currentState?.appendText('Error updating PC: $e');
    }
  }

  void _updateConsole() {
    try {
      if (!_isSimulatorInitialized) return;

      final output = _simulator.getConsoleOutput();
      _consoleKey.currentState?.clearConsole();

      // Split by newlines and append each line
      final lines = output.split('\n');
      for (final line in lines) {
        if (line.isNotEmpty) {
          _consoleKey.currentState?.appendText(line);
        }
      }
    } catch (e) {
      _consoleKey.currentState?.appendText('Error updating console: $e');
    }
  }

  void printRegs()
  {
      _consoleKey.currentState?.appendText("=> REGISTER FILE");
      final regData = _simulator.showReg();
      final regPairs = regData.split(';');
      for (final pair in regPairs)
      {
        _consoleKey.currentState?.appendText(pair);
      }
  }

  void printBuffers()
  {
    _consoleKey.currentState?.appendText("=> PIPELINE BUFFERS");
    final bufferData = _simulator.getPipelineState();
    final bufferPairs = bufferData.split(';');
    for (final pair in bufferPairs)
      {
        _consoleKey.currentState?.appendText(pair);
      }
  }

  void printBP()
  {
    _consoleKey.currentState?.appendText("=> BRANCH PREDICTOR");
    final btbData = _simulator.getBP();
    final btbPairs = btbData.split(';');
    for (final pair in btbPairs)
      {
        _consoleKey.currentState?.appendText(pair);
      }
  }

  final ScrollController _scrollController = ScrollController();

  @override
  void dispose() {
    memoryJumpController.dispose();
    if (_isSimulatorInitialized) {
      _simulator.cleanup();
    }
    super.dispose();
    _scrollController.dispose();
  }

  @override
  void didUpdateWidget(SimulatorTab oldWidget) {
    super.didUpdateWidget(oldWidget);

    // If code content changed, reload it
    if (widget.codeContent != oldWidget.codeContent &&
        _isSimulatorInitialized) {
      _loadCode();
    }

    // If currentPc changed, scroll to the new position
    if (widget.codeContent.isNotEmpty) {
      final lines = widget.codeContent.split('\n');
      for (int i = 0; i < lines.length; i++) {
        if (i * 4 == currentPc) {
          WidgetsBinding.instance.addPostFrameCallback((_) {
            _scrollToCurrentInstruction(i);
          });
          break;
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        // Main Content Area: Instructions, Console, Registers/Memory
        Expanded(
          flex: 4,
          child: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Left Panel - Instructions and Console Output
                Expanded(
                  flex: 3,
                  child: Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          ElevatedButton.icon(
                            onPressed: (!_isSimulatorInitialized || isRunning)
                                ? null
                                : _runSimulation,
                            icon: const Icon(Icons.play_arrow,
                                color: Colors.white),
                            label: const Text('Run',
                                style: TextStyle(color: Colors.white)),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.blue[900],
                              elevation: 4,
                            ),
                          ),
                          const SizedBox(width: 12),
                          ElevatedButton.icon(
                            onPressed: (!_isSimulatorInitialized || isRunning)
                                ? null
                                : _stepSimulation,
                            icon:
                                Icon(Icons.skip_next, color: Colors.blue[900]),
                            label: Text('Step',
                                style: TextStyle(color: Colors.blue[900])),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.white,
                              elevation: 4,
                            ),
                          ),
                          const SizedBox(width: 12),
                          ElevatedButton.icon(
                            onPressed: !_isSimulatorInitialized
                                ? null
                                : _resetSimulation,
                            icon: Icon(Icons.refresh, color: Colors.blue[900]),
                            label: Text('Reset',
                                style: TextStyle(color: Colors.blue[900])),
                            style: OutlinedButton.styleFrom(
                              backgroundColor: Colors.white,
                              elevation: 4,
                            ),
                          ),
                          const SizedBox(width: 12),
                          ElevatedButton.icon(
                            onPressed: () {
                              setState(() {
                                displayHex = !displayHex;
                              });
                            },
                            style: ElevatedButton.styleFrom(
                              backgroundColor:
                                  displayHex ? Colors.blue[900] : Colors.white,
                              elevation: 2,
                            ),
                            icon: Icon(
                              displayHex
                                  ? Icons.display_settings
                                  : Icons.display_settings,
                              color:
                                  displayHex ? Colors.white : Colors.blue[900],
                            ),
                            label: Text(
                              displayHex ? 'Dec' : 'Hex',
                              style: TextStyle(
                                color: displayHex
                                    ? Colors.white
                                    : Colors.blue[900],
                              ),
                            ),
                          )
                        ],
                      ),
                      const SizedBox(height: 16),
                      // Instructions Panel Card
                      Expanded(
                        flex: 1,
                        child: Card(
                          elevation: 8,
                          color: Colors.white,
                          clipBehavior: Clip.antiAlias,
                          child: Column(
                            children: [
                              Container(
                                padding: const EdgeInsets.all(12.0),
                                color: Colors.blue[900],
                                child: Row(
                                  children: const [
                                    Expanded(
                                      flex: 2,
                                      child: Text(
                                        'Address',
                                        textAlign: TextAlign.center,
                                        style: TextStyle(
                                          fontWeight: FontWeight.bold,
                                          color: Colors.white,
                                        ),
                                      ),
                                    ),
                                    Expanded(
                                      flex: 4,
                                      child: Text(
                                        'Machine Code',
                                        textAlign: TextAlign.center,
                                        style: TextStyle(
                                          fontWeight: FontWeight.bold,
                                          color: Colors.white,
                                        ),
                                      ),
                                    ),
                                    Expanded(
                                      flex: 4,
                                      child: Text(
                                        'Comment',
                                        textAlign: TextAlign.center,
                                        style: TextStyle(
                                          fontWeight: FontWeight.bold,
                                          color: Colors.white,
                                        ),
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              Expanded(
                                child: _buildInstructionPanel(),
                              ),
                            ],
                          ),
                        ),
                      ),
                      const SizedBox(height: 16),
                      // Console Output below the instruction panel.
                      Expanded(
                          flex: 1,
                          child: ConsoleOutputWidget(key: _consoleKey)),
                    ],
                  ),
                ),
                const SizedBox(width: 16),
                // Right Panel - Registers/Memory Card and Metrics Card
                Expanded(
                  flex: 2,
                  child: Card(
                    elevation: 8,
                    color: Colors.white,
                    clipBehavior: Clip.antiAlias,
                    child: DefaultTabController(
                      length: 4,
                      child: Column(
                        children: [
                          Container(
                            color: Colors.blue[900],
                            child: TabBar(
                              tabs: const [
                                Tab(text: 'REG'),
                                Tab(text: 'MEM'),
                                Tab(text: 'BTB/BHT'),
                                Tab(text: 'STATS'),
                              ],
                              labelColor: Colors.white,
                              unselectedLabelColor: Colors.grey[400],
                              indicatorColor: Colors.white,
                              indicatorWeight: 3,
                            ),
                          ),
                          Expanded(
                            child: TabBarView(
                              children: [
                                _buildRegisterView(),
                                _buildMemoryView(),
                                _buildBPView(),
                                _buildStatView()
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                )
              ],
            ),
          ),
        ),
        const SizedBox(width: 8),
        //Toggle button sidebar
        Expanded(
          flex:1,
          child: Container(
            color: Colors.white,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Container(
                  color: Colors.blue[900],
                  height: 50,
                  child: const Center(
                    child: Text(
                      'Knob Toolbar',
                      style: TextStyle(
                        color: Colors.white,
                        fontWeight: FontWeight.bold,
                        fontSize: 16,
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 8),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal:8.0),
                  child: SwitchListTile(
                    title: const Text('Pipelining'),
                    value: pipelineEnable,
                    onChanged: (bool value) {
                      setState(() {
                        pipelineEnable = value;
                        WidgetsBinding.instance.addPostFrameCallback((_) {
                          _initializeSimulator();
                        });
                      });
                    },
                    activeTrackColor: Colors.blue[900],
                  ),
                ),
                const SizedBox(height: 4),
                const Divider(
                  color: Colors.grey,
                  height: 1,
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal:8.0),
                  child: SwitchListTile(
                    title: const Text('Data Forwarding'),
                    value: dataForwardingEnable,
                    onChanged: (bool value) {
                      setState(() {
                        dataForwardingEnable = value;
                        WidgetsBinding.instance.addPostFrameCallback((_) {
                          _simulator.toggleForwarding(dataForwardingEnable);
                        });
                      });
                    },
                    activeTrackColor: Colors.blue[900],
                  ),
                ),
                const SizedBox(height: 4),
                const Divider(
                  color: Colors.grey,
                  height: 1,
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal:8.0),
                  child: SwitchListTile(
                    title: const Text('Print Pipeline Buffers'),
                    value: showBuffers,
                    onChanged: (bool value) {
                      setState(() {
                        showBuffers = value;
                      });
                    },
                    activeTrackColor: Colors.blue[900],
                  ),
                ),
                const SizedBox(height: 4),
                const Divider(
                  color: Colors.grey,
                  height: 1,
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal:8.0),
                  child: SwitchListTile(
                    title: const Text('Print Register File'),
                    value: showReg,
                    onChanged: (bool value) {
                      setState(() {
                        showReg = value;
                      });
                    },
                    activeTrackColor: Colors.blue[900],
                  ),
                ),
                const SizedBox(height: 4),
                const Divider(
                  color: Colors.grey,
                  height: 1,
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal:8.0),
                  child: SwitchListTile(
                    title: const Text('Print Branch Predictor'),
                    value: showBP,
                    onChanged: (bool value) {
                      setState(() {
                        showBP = value;
                      });
                    },
                    activeTrackColor: Colors.blue[900],
                  ),
                ),
                const SizedBox(height: 4),
                const Divider(
                  color: Colors.grey,
                  height: 1,
                ),
                const SizedBox(height: 4),
              ],
            ),
          )
        )
      ],
    );
  }

  // Simulation controls
  void _runSimulation() {
    if (widget.codeContent.isEmpty) {
      _consoleKey.currentState?.appendText('Error: No code to execute');
      return;
    }

    setState(() {
      isRunning = true;
    });

    // Clear the console and then append the first log entry.
    _consoleKey.currentState?.clearConsole();
    _consoleKey.currentState?.appendText('Running simulation...');

    try {
      _simulator.run();

      // Update UI after simulation completes
      _updateRegisters();
      _updateMemory();
      _updateBP();
      _updatePC();
      _updateConsole();

      _consoleKey.currentState?.appendText('=> Simulation complete');

      setState(() {
        isRunning = false;
      });
    } catch (e) {
      _consoleKey.currentState?.appendText('Error during simulation: $e');
      setState(() {
        isRunning = false;
      });
    }
  }

  void _stepSimulation() {
    if (widget.codeContent.isEmpty) {
      _consoleKey.currentState?.appendText('Error: No code to execute');
      return;
    }

    try {
      final continueExecution = _simulator.step();

      // Update UI after step
      _updateRegisters();
      _updateMemory();
      _updateBP();
      if(!pipelineEnable)_updatePC();
      _updateConsole();
      if(showReg)printRegs();
      if (showBuffers) printBuffers();
      if (showBP) printBP();

      if (!continueExecution) {
        _consoleKey.currentState?.appendText('=> Simulation complete');
      }
    } catch (e) {
      _consoleKey.currentState?.appendText('Error during step: $e');
    }
  }

  void _resetSimulation() {
    try {
      _simulator.reset();
      _consoleKey.currentState?.clearConsole();
      _consoleKey.currentState?.appendText('Simulator reset');

      // Update UI after reset
      _updateRegisters();
      _updateMemory();
      _updateBP();
      _updatePC();

      if (widget.codeContent.isNotEmpty) {
        _loadCode();
      }

      setState(() {
        currentPc = 0;
      });
    } catch (e) {
      _consoleKey.currentState?.appendText('Error resetting simulator: $e');
    }
  }

  void _scrollToCurrentInstruction(int index) {
    if (!_scrollController.hasClients) return;

    // Calculate the position to scroll to
    // This assumes all items have the same height
    // You may need to adjust this calculation based on your actual item heights
    final itemHeight = 40.0; // Estimate of your item height
    final offset = index * itemHeight;

    // Get the current scroll position and viewport height
    final position = _scrollController.position;
    final viewportHeight = position.viewportDimension;

    // Calculate a position that centers the item if possible
    final scrollTo = offset - (viewportHeight / 2) + (itemHeight / 2);

    // Ensure we don't scroll beyond bounds
    final maxScroll = position.maxScrollExtent;
    final minScroll = position.minScrollExtent;
    final boundedOffset = scrollTo.clamp(minScroll, maxScroll);

    // Animate to the position
    _scrollController.animateTo(
      boundedOffset,
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeInOut,
    );
  }

  Widget _buildInstructionPanel() {
    if (mcCode.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.code_off_rounded, size: 60, color: Colors.grey[700]),
            const SizedBox(height: 16),
            Text(
              'No code loaded',
              style: TextStyle(
                color: Colors.grey[800],
                fontSize: 16,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Switch to Editor tab to add code',
              style: TextStyle(
                color: Colors.grey[600],
                fontSize: 14,
              ),
            ),
          ],
        ),
      );
    }

    final lines = mcCode.split('\n');

    // Find the index of the current instruction
    int currentIndex = -1;
    for (int i = 0; i < lines.length; i++) {
      if (i * 4 == currentPc) {
        currentIndex = i;
        break;
      }
    }

    // Scroll to the current instruction after the build is complete
    if (currentIndex != -1) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _scrollToCurrentInstruction(currentIndex);
      });
    }

    return ListView.builder(
      controller: _scrollController, // Add the controller here
      itemCount: lines.length,
      itemBuilder: (context, index) {
        final rawLine = lines[index].trim();
        if (rawLine.isEmpty) return const SizedBox.shrink();

        String machineCode = "";
        String asmInstruction = "";

        String lineWithoutDebug = rawLine.split('#')[0].trim();
        final parts = lineWithoutDebug.split(',');
        final tokens = parts[0].trim().split(RegExp(r'\s+'));
        if (tokens.length > 1) {
          machineCode = tokens[1].trim();
        }
        asmInstruction = parts.sublist(1).join(',').trim();

        final address =
            '0x${(index * 4).toRadixString(16).padLeft(8, '0').toUpperCase()}';
        final isCurrentInstruction = index * 4 == currentPc;

        return Container(
          color: isCurrentInstruction
              ? Colors.yellow.shade100
              : (index % 2 == 0 ? Colors.grey.shade50 : Colors.white),
          padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 12),
          child: Row(
            children: [
              // Address column
              Expanded(
                flex: 2,
                child: Text(
                  address,
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontFamily: 'SpaceMono',
                    fontWeight: FontWeight.bold,
                    color: Colors.blue.shade700,
                  ),
                ),
              ),
              // Machine Code column
              Expanded(
                flex: 4,
                child: Text(
                  machineCode,
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontFamily: 'SpaceMono',
                    fontWeight: isCurrentInstruction
                        ? FontWeight.bold
                        : FontWeight.normal,
                  ),
                ),
              ),
              // Comment column
              Expanded(
                flex: 4,
                child: Text(
                  asmInstruction,
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontFamily: 'SpaceMono',
                    color: Colors.green.shade700,
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildRegisterView() {
    return ListView.builder(
      itemCount: registers.length,
      itemBuilder: (context, index) {
        final register = registers[index];
        final hexValue = register['value'].toString();

        return Container(
          decoration: BoxDecoration(
            color: index % 2 == 0 ? Colors.grey.shade50 : Colors.white,
            border: Border(
              bottom: BorderSide(color: Colors.grey.shade200),
            ),
          ),
          padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 16),
          child: Row(
            children: [
              // Expanded for the register label
              Expanded(
                flex: 1,
                child: Center(
                  child: Text(
                    register['name'],
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      color: Colors.blue.shade800,
                    ),
                  ),
                ),
              ),
              // Expanded for the hex value container
              Expanded(
                flex: 1,
                child: Center(
                  child: Container(
                    padding:
                        const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                    decoration: BoxDecoration(
                      color: Colors.grey.shade100,
                      borderRadius: BorderRadius.circular(4),
                      border: Border.all(color: Colors.grey.shade300),
                    ),
                    child: Text(
                      displayHex
                          ? hexValue
                          : int.parse(hexValue, radix: 16).toString(),
                      style: TextStyle(
                        fontFamily: 'SpaceMono',
                        fontWeight: hexValue != "00000000"
                            ? FontWeight.bold
                            : FontWeight.normal,
                        color: hexValue != "00000000"
                            ? Colors.blue.shade700
                            : Colors.grey.shade700,
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildStatView() {
    String stats='';
    if(_isSimulatorInitialized)stats = _simulator.getStats();
    if (stats.isEmpty) {
      return Center(
        child: Text(
          'No Statistcs available',
          style: TextStyle(color: Colors.grey.shade700),
        ),
      );
    }

    final stpairs = stats.split(';').map((stat) {
      final parts = stat.split(':');
      return {
        'label': parts[0],
        'value': parts.length > 1 ? parts[1] : 'N/A',
      };
    }).toList();

    return Column(
      children: [
        Container(
          width: double.infinity,
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: Colors.grey.shade100,
            border: const Border(
              bottom: BorderSide(color: Colors.grey),
            ),
          ),
          child: const Text(
            'Pipeline Statistics',
            textAlign: TextAlign.center,
            style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
          ),
        ),
        Expanded(
          child: SingleChildScrollView(
            scrollDirection: Axis.vertical,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: DataTable(
                columnSpacing: 110,
                columns: const [
                  DataColumn(label: Text('Statistic')),
                  DataColumn(label: Text('Value')),
                ],
                rows: stpairs.map((stat) {
                  return DataRow(cells: [
                    DataCell(Text(
                      stat['label']!,
                      textAlign: TextAlign.center,
                      style: TextStyle(color: Colors.grey[600]),
                    )),
                    DataCell(Text(
                      stat['value']!,
                      textAlign: TextAlign.center,
                      style: TextStyle(
                          fontFamily: 'SpaceMono', color: Colors.blue[800]),
                    )),
                  ]);
                }).toList(),
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildBPView() {
    // If both maps are empty, show a placeholder message
    if (BTB.isEmpty && BHT.isEmpty) {
      return Center(
        child: Text(
          'No Branch Predictor data available',
          style: TextStyle(color: Colors.grey.shade700),
        ),
      );
    }

    // Collect all PC addresses from both maps
    final addresses = {...BTB.keys, ...BHT.keys}.toList();
    addresses.sort((a, b) => a.compareTo(b));

    return Column(
      children: [
        Container(
          width: double.infinity,
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: Colors.grey.shade100,
            border: const Border(
              bottom: BorderSide(color: Colors.grey),
            ),
          ),
          child: const Text(
            'Branch Predictor',
            textAlign: TextAlign.center,
            style: TextStyle(
              fontWeight: FontWeight.bold,
              fontSize: 16,
            ),
          ),
        ),
        Expanded(
          child: SingleChildScrollView(
            scrollDirection: Axis.vertical,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: DataTable(
                columns: const [
                  DataColumn(label: Text('PC Address')),
                  DataColumn(label: Text('BTB Entry')),
                  DataColumn(label: Text('BHT Entry')),
                ],
                rows: addresses.map((pc) {
                  final btbEntry = BTB[pc] ?? 'N/A';
                  final bhtEntry = (BHT[pc] == true ? "Taken" : "Not Taken");
                  return DataRow(
                    cells: [
                      DataCell(Text(pc,
                          textAlign: TextAlign.center,
                          style: TextStyle(fontFamily: 'SpaceMono'))),
                      DataCell(Text(
                        btbEntry,
                        textAlign: TextAlign.center,
                        style: TextStyle(fontFamily: 'SpaceMono'),
                      )),
                      DataCell(Text(
                        bhtEntry,
                        textAlign: TextAlign.center,
                        style: TextStyle(
                            fontFamily: 'SpaceMono',
                            color: (bhtEntry == "Taken")
                                ? Colors.blue[800]
                                : Colors.grey[700]),
                      )),
                    ],
                  );
                }).toList(),
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildMemoryView() {
    // Generate a list of addresses for the current page.
    final visibleAddresses = List.generate(
      memoryPageSize,
      (index) => memoryStartAddress + index * 4,
    ).reversed.toList(); // Reverse if you want descending display

    return Column(
      children: [
        // Segment selector and memory navigation controls.
        Container(
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: Colors.grey.shade100,
            border: const Border(
              bottom: BorderSide(color: Colors.grey),
            ),
          ),
          child: Column(
            children: [
              // ─────────────────────────────────────────────────────────────────
              // Segment selector: Data, Text, Stack, Heap
              // When selected, jump to that segment's base address.
              // ─────────────────────────────────────────────────────────────────
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: segments.map((seg) {
                      final isSelected = seg == currentSegment;
                      return Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 4.0),
                        child: ChoiceChip(
                          label: Text(seg),
                          selectedColor: Colors.blue[900],
                          labelStyle: TextStyle(
                            color: isSelected ? Colors.white : Colors.blue[900],
                          ),
                          showCheckmark: false,
                          backgroundColor: Colors.white,
                          selected: isSelected,
                          onSelected: (selected) {
                            setState(() {
                              currentSegment = seg;
                              // Jump to the segment's base address:
                              int segmentAddress;
                              switch (seg) {
                                case 'Data':
                                  segmentAddress = 0x10000000;
                                  break;
                                case 'Text':
                                  segmentAddress = 0x00000000;
                                  break;
                                case 'Stack':
                                  segmentAddress = 0x7FFFFFFC;
                                  break;
                                default:
                                  segmentAddress = memoryMinAddress;
                              }
                              memoryStartAddress = segmentAddress;
                              _updateMemory();
                            });
                          },
                        ),
                      );
                    }).toList(),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              // ─────────────────────────────────────────────────────────────────
              // Memory navigation controls + jump to address field
              // ─────────────────────────────────────────────────────────────────
              Row(
                children: [
                  // Down button: Moves to lower addresses.
                  IconButton(
                    icon: Icon(
                      Icons.keyboard_arrow_down,
                      color: memoryStartAddress > memoryMinAddress
                          ? Colors.white
                          : Colors.blue[900],
                    ),
                    onPressed: memoryStartAddress > memoryMinAddress
                        ? () {
                            setState(() {
                              // Decrement page (ensuring we don't go below minimum)
                              memoryStartAddress = (memoryStartAddress -
                                      memoryPageSize * 4)
                                  .clamp(memoryMinAddress, memoryMaxAddress);
                              _updateMemory();
                            });
                          }
                        : null,
                    tooltip: 'Down',
                    style: IconButton.styleFrom(
                      backgroundColor: Colors.blue.shade900,
                    ),
                  ),
                  const SizedBox(width: 8),
                  // Up button: Moves to higher addresses.
                  IconButton(
                    icon: Icon(
                      Icons.keyboard_arrow_up,
                      color: (memoryStartAddress + memoryPageSize * 4) <=
                              memoryMaxAddress
                          ? Colors.white
                          : Colors.blue[900],
                    ),
                    onPressed: (memoryStartAddress + memoryPageSize * 4) <=
                            memoryMaxAddress
                        ? () {
                            setState(() {
                              memoryStartAddress =
                                  (memoryStartAddress + memoryPageSize * 4)
                                      .clamp(
                                          memoryMinAddress,
                                          memoryMaxAddress -
                                              memoryPageSize * 4 +
                                              4);
                              _updateMemory();
                            });
                          }
                        : null,
                    tooltip: 'Up',
                    style: IconButton.styleFrom(
                      backgroundColor: Colors.blue.shade900,
                    ),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: memoryJumpController,
                            decoration: InputDecoration(
                              labelText: 'Jump to address',
                              labelStyle: TextStyle(
                                  color: Colors.blue.shade900, fontSize: 14),
                              isDense: true,
                              fillColor: Colors.white,
                              filled: true,
                              prefixIcon: Icon(Icons.search,
                                  color: Colors.blue.shade900),
                              focusedBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(8),
                                borderSide: BorderSide(
                                  color: Colors.blue.shade900,
                                  width: 2,
                                ),
                              ),
                              enabledBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(8),
                                borderSide: BorderSide(
                                  color: Colors.blue.shade900,
                                  width: 2,
                                ),
                              ),
                            ),
                            style: const TextStyle(fontFamily: 'SpaceMono'),
                          ),
                        ),
                        const SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: () {
                            final input = memoryJumpController.text.trim();
                            if (input.isEmpty) return;

                            int? address;
                            if (input.startsWith('0x')) {
                              address =
                                  int.tryParse(input.substring(2), radix: 16);
                            } else {
                              address = int.tryParse(input);
                            }

                            if (address != null &&
                                address >= memoryMinAddress &&
                                address <= memoryMaxAddress) {
                              setState(() {
                                // Align the jump address to a 4-byte boundary.
                                memoryStartAddress = (address! - (address % 4))
                                    .clamp(
                                        memoryMinAddress,
                                        memoryMaxAddress -
                                            memoryPageSize * 4 +
                                            4);
                                _updateMemory();
                              });
                              memoryJumpController.clear();
                            } else {
                              Flushbar(
                                messageText: Text(
                                  "Invalid address: $input",
                                  textAlign: TextAlign.center,
                                  style: TextStyle(color: Colors.white),
                                ),
                                duration: const Duration(seconds: 3),
                                flushbarPosition: FlushbarPosition.TOP,
                                flushbarStyle: FlushbarStyle.FLOATING,
                                backgroundColor: Colors.red,
                                icon: Icon(
                                  Icons.highlight_off,
                                  color: Colors.white,
                                ),
                                margin: const EdgeInsets.all(8),
                                borderRadius: BorderRadius.circular(10),
                                maxWidth: 300,
                              ).show(context);
                            }
                          },
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.blue[900],
                            elevation: 2,
                          ),
                          child: const Text('Jump',
                              style: TextStyle(color: Colors.white)),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        // ──────────────────────────────────────────────────────────────────────
        // DataTable showing memory words and their bytes
        // ──────────────────────────────────────────────────────────────────────
        Expanded(
          child: SingleChildScrollView(
            scrollDirection: Axis.vertical,
            reverse: true,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: SizedBox(
                // No width: double.infinity here to avoid infinite-width errors.
                child: DataTable(
                  columnSpacing: 50,
                  headingRowHeight: 32,
                  columns: const [
                    DataColumn(
                        label: Text(
                      'Address',
                      textAlign: TextAlign.center,
                    )),
                    DataColumn(
                        label: Text(
                      '+3',
                      textAlign: TextAlign.center,
                    )),
                    DataColumn(
                        label: Text(
                      '+2',
                      textAlign: TextAlign.center,
                    )),
                    DataColumn(
                        label: Text(
                      '+1',
                      textAlign: TextAlign.center,
                    )),
                    DataColumn(
                        label: Text(
                      '+0',
                      textAlign: TextAlign.center,
                    )),
                  ],
                  rows: visibleAddresses.map((address) {
                    // Convert the address to a hex string.
                    final addressHex =
                        '0x${address.toRadixString(16).padLeft(8, '0').toUpperCase()}';
                    // Get the word from the memory map, defaulting to "0x00000000"
                    final wordStr = memoryMap[addressHex] ?? '00000000';
                    // Parse the word to an integer.
                    final word = int.tryParse(wordStr, radix: 16) ?? 0;

                    // Extract each byte from the 32-bit word.
                    final b3 = (word >> 24) & 0xFF;
                    final b2 = (word >> 16) & 0xFF;
                    final b1 = (word >> 8) & 0xFF;
                    final b0 = word & 0xFF;

                    // Convert each byte to uppercase hex (e.g. "0A")
                    String toHex(int b) => displayHex
                        ? b.toRadixString(16).padLeft(2, '0').toUpperCase()
                        : b.toString().padLeft(2, '0');

                    return DataRow(
                      cells: [
                        DataCell(
                          Text(
                            addressHex,
                            style: const TextStyle(fontFamily: 'SpaceMono'),
                          ),
                        ),
                        DataCell(Text(toHex(b3),
                            style: const TextStyle(fontFamily: 'SpaceMono'))),
                        DataCell(Text(toHex(b2),
                            style: const TextStyle(fontFamily: 'SpaceMono'))),
                        DataCell(Text(toHex(b1),
                            style: const TextStyle(fontFamily: 'SpaceMono'))),
                        DataCell(Text(toHex(b0),
                            style: const TextStyle(fontFamily: 'SpaceMono'))),
                      ],
                    );
                  }).toList(),
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}
