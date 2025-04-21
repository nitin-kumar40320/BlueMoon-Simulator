import 'dart:async';
import 'package:another_flushbar/flushbar.dart';
import 'package:flutter/material.dart';
import 'package:riscv_simulator/wasm_interop.dart';

Map<String, Color> instructionColors = {};
final List<Color> availableColors = [
  Colors.red.shade300,
  Colors.green.shade300,
  Colors.blue.shade300,
  Colors.orange.shade300,
  Colors.purple.shade300,
  Colors.yellow.shade300,
  Colors.teal.shade300,
  Colors.brown.shade300,
  Colors.cyan.shade300,
  Colors.lime.shade300,
];
int colorIndex = 0;

class PipelineBlockDiagram extends StatefulWidget {
  final String codeContent;

  const PipelineBlockDiagram({
    super.key,
    required this.codeContent,
  });

  @override
  State<PipelineBlockDiagram> createState() => _PipelineBlockDiagramState();
}

class _PipelineBlockDiagramState extends State<PipelineBlockDiagram> {
  late Simulator _simulator;
  bool dataForwardingEnabled = true; 
  bool _isSimulatorInitialized = false;
  Timer? _runTimer; // Timer for the Run button
  bool _isRunning = false;

  // Pipeline stages
  Map<String, String> ifStage = {'instruction': '', 'pc': ''};
  Map<String, String> idStage = {
    'instruction': '',
    'rs1': '',
    'rs2': '',
    'rd': ''
  };
  Map<String, String> exStage = {
    'instruction': '',
    'aluResult': '',
    'operation': ''
  };
  Map<String, String> memStage = {
    'instruction': '',
    'memAddress': '',
    'memData': ''
  };
  Map<String, String> wbStage = {
    'instruction': '',
    'writeData': '',
    'writeReg': ''
  };

  // Buffers
  Map<String, dynamic> ifIdBuffer = {};
  Map<String, dynamic> idExBuffer = {};
  Map<String, dynamic> exMemBuffer = {};
  Map<String, dynamic> memWbBuffer = {};

  List<ForwardingPath> forwardingPaths = [];
  List<List<String>> hazards = [];

  @override
  void initState() {
    super.initState();
    _initializeSimulator();
  }

  @override
  void dispose() {
    _runTimer?.cancel(); // Cancel the timer if it's running
    if (_isSimulatorInitialized) _simulator.cleanup();
    super.dispose();
  }

  @override
  void didUpdateWidget(PipelineBlockDiagram old) {
    super.didUpdateWidget(old);
    if (!_isSimulatorInitialized) return;
    if (widget.codeContent != old.codeContent) {
      _loadCode();
    }
    _simulator.toggleForwarding(dataForwardingEnabled);
    _updatePipelineState();
  }

  Future<void> _initializeSimulator() async {
    try {
      _simulator = await RiscVPipelinedSimulator.create();
      _simulator.init();
      setState(() => _isSimulatorInitialized = true);

      if (widget.codeContent.isNotEmpty) _loadCode();
      _simulator.toggleForwarding(dataForwardingEnabled);
      _updatePipelineState();
    } catch (e) {
      print('Error initializing simulator: $e');
    }
  }

  void _loadCode() {
    try {
      final mc = _simulator.assemble(widget.codeContent);
      _simulator.loadCode(mc);
      _updatePipelineState();
    } catch (e) {
      print('Error loading code: $e');
    }
  }

  void _stepSimulation() {
    if (!_isSimulatorInitialized || widget.codeContent.isEmpty) return;
    try {
      if (!_simulator.step()) {
        print('Simulation complete');
        _stopRun(); // Stop the timer if the simulation is complete
      }
      _updatePipelineState();
    } catch (e) {
      print('Error during step: $e');
    }
  }

  void _runSimulation() {
    if (!_isSimulatorInitialized || _isRunning) return;
    setState(() => _isRunning = true);
    _runTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      _stepSimulation();
    });
  }

  void _stopRun() {
    _runTimer?.cancel();
    setState(() => _isRunning = false);
  }

  void _resetSimulation() {
    if (!_isSimulatorInitialized) return;
    try {
      _simulator.reset();
      instructionColors.clear();
      colorIndex = 0;
      _updatePipelineState();
      if (widget.codeContent.isNotEmpty) _loadCode();
    } catch (e) {
      print('Error resetting simulator: $e');
    }
  }

  void _updatePipelineState() {
    try {
      final state = _simulator.getPipelineState();
      _parsePipelineState(state);
    } catch (e) {
      print('Error updating pipeline state: $e');
    }
  }

  void _parsePipelineState(String s) {
    forwardingPaths.clear();
    hazards.clear();

    for (var part in s.split(';')) {
      if (part.isEmpty) continue;
      final kv = part.split(':');
      if (kv.length < 2) continue;

      final instruction = kv[1].split(',').first; // Extract the instruction
      if (instruction.isNotEmpty && !instructionColors.containsKey(instruction)) {
        instructionColors[instruction] = availableColors[colorIndex%availableColors.length];
        print('$instruction, ${instructionColors[instruction]}');
        colorIndex++;
      }


      switch (kv[0]) {
        case 'IF':
          _parseIfStage(kv[1]);
          break;
        case 'ID':
          _parseIdStage(kv[1]);
          break;
        case 'EX':
          _parseExStage(kv[1]);
          break;
        case 'MEM':
          _parseMemStage(kv[1]);
          break;
        case 'WB':
          _parseWbStage(kv[1]);
          break;
        case 'IF/ID':
          _parseIfIdBuffer(kv[1]);
          break;
        case 'ID/EX':
          _parseIdExBuffer(kv[1]);
          break;
        case 'EX/MEM':
          _parseExMemBuffer(kv[1]);
          break;
        case 'MEM/WB':
          _parseMemWbBuffer(kv[1]);
          break;
        case 'HAZ':
          _parseHazards(kv[1]);
        case 'FWD':
          _parseForwardingPaths(kv[1]);
          break;
      }
    }
    setState(() {});
  }

  void _parseHazards(String data) {
    var haz = data.split('-');
    int dh=0,ch=0;
    for (var hazard in haz) {
      if (hazard.isEmpty) continue;
      final parts = hazard.split(',');
      if (parts[0] == 'Data') {
        dh++;
        List<String> res = [];
        res.add('FROM: ${parts[1]} , INSTR: ${parts[2]}');
        res.add('TO: ${parts[3]} , INSTR: ${parts[4]}');
        hazards.add(res);
      } else {
        ch++;
        List<String> res = [];
        res.add('PREDICTED: ${parts[1]}');
        res.add('ACTUAL: ${parts[2]}');
        hazards.add(res);
      }
    }
    if(dh>0 && ch>0)
    {
      _showHazard('DATA AND CONTROL HAZARDS DETECTED !!!', 'Flushing and ${dataForwardingEnabled?'Data Forwarding':'Stalling'} done');
    } else if (dh > 0) {
      _showHazard('DATA HAZARD DETECTED !!!', '${dataForwardingEnabled?'Data Forwarding':'Stalling'} done');
    } else if (ch > 0) {
      _showHazard('CONTROL HAZARD DETECTED !!!', 'Flushing the Pipeline');
    }
  }

  void _showHazard(String title, String message)
  {
    Flushbar(
      messageText: Text(
        message,
        textAlign: TextAlign.center,
        style: TextStyle(color: Colors.grey[700]),
      ),
      duration: const Duration(seconds: 3),
      flushbarPosition: FlushbarPosition.TOP,
      flushbarStyle: FlushbarStyle.FLOATING,
      backgroundColor: Colors.black,
      icon: Padding(
        padding: const EdgeInsets.all(10.0),
        child: Icon(Icons.error, size: 40, color: Colors.red),
      ),
      titleText: Text(title, textAlign: TextAlign.center, style: const TextStyle(color: Colors.white)),
      margin: const EdgeInsets.all(8),
      borderRadius: BorderRadius.circular(10),
      maxWidth: 300,
    ).show(context);

  }

  void _parseIfStage(String d) {
    final f = d.split(',');
    ifStage = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'pc': f.length > 1 ? f[1] : '',
    };
  }

  void _parseIdStage(String d) {
    final f = d.split(',');
    idStage = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'rs1': f.length > 1 ? f[1] : '',
      'rs2': f.length > 2 ? f[2] : '',
      'rd': f.length > 3 ? f[3] : '',
    };
  }

  void _parseExStage(String d) {
    final f = d.split(',');
    exStage = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'operation': f.length > 1 ? f[1] : '',
      'aluResult': f.length > 2 ? f[2] : '',
    };
  }

  void _parseMemStage(String d) {
    final f = d.split(',');
    memStage = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'memAddress': f.length > 1 ? f[1] : '',
      'memData': f.length > 2 ? f[2] : '',
    };
  }

  void _parseWbStage(String d) {
    final f = d.split(',');
    wbStage = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'writeReg': f.length > 1 ? f[1] : '',
      'writeData': f.length > 2 ? f[2] : '',
    };
  }

  void _parseIfIdBuffer(String d) {
    final f = d.split(',');
    ifIdBuffer = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'pc': f.length > 1 ? f[1] : '',
    };
  }

  void _parseIdExBuffer(String d) {
    final f = d.split(',');
    idExBuffer = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'rs1': f.length > 1 ? f[1] : '',
      'rs2': f.length > 2 ? f[2] : '',
      'rd': f.length > 3 ? f[3] : '',
      'imm': f.length > 4 ? f[4] : '',
    };
  }

  void _parseExMemBuffer(String d) {
    final f = d.split(',');
    exMemBuffer = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'aluResult': f.length > 1 ? f[1] : '',
      'rs2Value': f.length > 2 ? f[2] : '',
      'rd': f.length > 3 ? f[3] : '',
    };
  }

  void _parseMemWbBuffer(String d) {
    final f = d.split(',');
    memWbBuffer = {
      'instruction': f.isNotEmpty ? f[0] : '',
      'memData': f.length > 1 ? f[1] : '',
      'aluResult': f.length > 2 ? f[2] : '',
      'rd': f.length > 3 ? f[3] : '',
    };
  }

  void _parseForwardingPaths(String d) {
    for (var p in d.split(',')) {
      if (p.isEmpty) continue;
      final pts = p.split('-');
      if (pts.length == 2) {
        forwardingPaths.add(ForwardingPath(from: pts[0], to: pts[1]));
      }
    }
  }

  @override
  Widget build(BuildContext ctx) {
    return Padding(
      padding: const EdgeInsets.all(24.0),
      child: Column(
        children: [
          // controls
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              ElevatedButton.icon(
                onPressed: _isSimulatorInitialized ? _stepSimulation : null,
                icon: const Icon(Icons.skip_next, color: Colors.white),
                label:
                    const Text('Step', style: TextStyle(color: Colors.white)),
                style:
                    ElevatedButton.styleFrom(backgroundColor: Colors.blue[900]),
              ),
              const SizedBox(width: 16),
              ElevatedButton.icon(
                onPressed: _isSimulatorInitialized
                    ? (_isRunning ? _stopRun : _runSimulation)
                    : null,
                icon: Icon(
                  _isRunning ? Icons.pause : Icons.play_arrow,
                  color: Colors.white,
                ),
                label: Text(
                  _isRunning ? 'Pause' : 'Run',
                  style: const TextStyle(color: Colors.white),
                ),
                style:
                    ElevatedButton.styleFrom(backgroundColor: Colors.blue[900]),
              ),
              const SizedBox(width: 16),
              ElevatedButton.icon(
                onPressed: _isSimulatorInitialized ? _resetSimulation : null,
                icon: Icon(Icons.refresh, color: Colors.blue[900]),
                label: Text('Reset', style: TextStyle(color: Colors.blue[900])),
                style: OutlinedButton.styleFrom(backgroundColor: Colors.white),
              ),
              const SizedBox(width: 16),
              //Add button to toggle data forwarding
              ElevatedButton.icon(
                onPressed: () {
                  setState(() {
                    dataForwardingEnabled = !dataForwardingEnabled;
                    WidgetsBinding.instance.addPostFrameCallback((_) {
                          _simulator.toggleForwarding(dataForwardingEnabled);
                    });
                  });
                },
                icon: Icon(
                  dataForwardingEnabled ? Icons.dnd_forwardslash : Icons.forward,
                  color: Colors.white,
                ),
                label: Text(
                  !dataForwardingEnabled ? 'Data Forwarding' : 'Stalling',
                  style: const TextStyle(color: Colors.white),
                ),
                style: ElevatedButton.styleFrom(
                    backgroundColor:
                        dataForwardingEnabled ? Colors.blue[900] : Colors.red),
              ),
            ],
          ),
          const SizedBox(height: 16),
          // diagram only
          Expanded(
            flex: 5,
            child: Center(
              child: Container(
                width: double.infinity,
                height: double.infinity,
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: Colors.white,
                  borderRadius: BorderRadius.circular(12),
                  boxShadow: [
                    BoxShadow(
                        color: Colors.black26,
                        blurRadius: 8,
                        offset: Offset(0, 4))
                  ],
                ),
                child: _isSimulatorInitialized
                    ? SingleChildScrollView(
                        scrollDirection: Axis.horizontal,
                        child: SimplePipelineDiagram(
                            ifStage: ifStage,
                            idStage: idStage,
                            exStage: exStage,
                            memStage: memStage,
                            wbStage: wbStage,
                            ifIdBuffer: ifIdBuffer,
                            idExBuffer: idExBuffer,
                            exMemBuffer: exMemBuffer,
                            memWbBuffer: memWbBuffer),
                      )
                    : const SizedBox(
                        height: 200,
                        child: Center(child: CircularProgressIndicator()),
                      ),
              ),
            ),
          ),
          const SizedBox(height: 16),
          Expanded(
            flex: 1,
            child: Row(
              children: [
                Expanded(
                  flex: 1,
                  child: Container(
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(8),
                      boxShadow: [
                        BoxShadow(
                          color: Colors.black12,
                          blurRadius: 4,
                          offset: Offset(0, 2),
                        ),
                      ],
                    ),
                    height: double.infinity,
                    child: Column(
                      children: [
                        Container(
                          padding:
                              EdgeInsets.symmetric(vertical: 8, horizontal: 12),
                          decoration: BoxDecoration(
                            color: Color(0xFF0D47A1), // blue[900]
                            borderRadius: BorderRadius.only(
                              topLeft: Radius.circular(8),
                              topRight: Radius.circular(8),
                            ),
                          ),
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Text(
                            'Data Forwarding Paths',
                            style: TextStyle(
                              fontSize: 16,
                              fontWeight: FontWeight.bold,
                              color: Colors.white,
                            ),
                          ),
                            ],
                          ),
                        ),
                        Expanded(
                          child: forwardingPaths.isEmpty
                              ? Center(
                                  child: Text(
                                    'No Forwarding Paths',
                                    style: TextStyle(
                                      color: Colors.grey[600],
                                      fontSize: 13,
                                    ),
                                  ),
                                )
                              : Padding(
                                padding: const EdgeInsets.all(8.0),
                                child: ListView.builder(
                                    itemCount: forwardingPaths.length,
                                    padding: EdgeInsets.symmetric(
                                        vertical: 1, horizontal: 8),
                                itemBuilder: (context, index) {
                                  final path = forwardingPaths[index];
                                  return Padding(
                                    padding: const EdgeInsets.symmetric(horizontal: 4),
                                    child: Text(
                                      '${path.from} -> ${path.to}',
                                      textAlign: TextAlign.center,
                                      style: TextStyle(
                                        fontSize: 14,
                                        color: Colors.black87,
                                      ),
                                    ),
                                  );
                                },
                                  ),
                              ),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  flex: 1,
                  child: Container(
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(8),
                      boxShadow: [
                        BoxShadow(
                          color: Colors.black12,
                          blurRadius: 4,
                          offset: Offset(0, 2),
                        ),
                      ],
                    ),
                    height: double.infinity,
                    child: Column(
                      children: [
                        Container(
                          padding:
                              EdgeInsets.symmetric(vertical: 8, horizontal: 12),
                          decoration: BoxDecoration(
                            color: Color(0xFF0D47A1), // blue[900]
                            borderRadius: BorderRadius.only(
                              topLeft: Radius.circular(8),
                              topRight: Radius.circular(8),
                            ),
                          ),
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Text(
                                'Hazard Detection',
                                style: TextStyle(
                                  fontSize: 14,
                                  fontWeight: FontWeight.w600,
                                  color: Colors.white,
                                ),
                              ),
                            ],
                          ),
                        ),
                        Expanded(
                          child: hazards.isEmpty
                              ? Center(
                                  child: Text(
                                    'No hazards detected',
                                    style: TextStyle(
                                      color: Colors.grey[600],
                                      fontSize: 13,
                                    ),
                                  ),
                                )
                              : Padding(
                                padding: const EdgeInsets.all(8.0),
                                child: ListView.builder(
                                    itemCount: hazards.length,
                                    padding: EdgeInsets.symmetric(
                                        vertical: 1, horizontal: 8),
                                    itemBuilder: (context, index) {
                                      final hazard = hazards[index];
                                      return Row(
                                        mainAxisAlignment:
                                            MainAxisAlignment.center,
                                        children: [
                                          ...hazard
                                              .map((h) => Padding(
                                                    padding:
                                                        const EdgeInsets.symmetric(horizontal:4),
                                                      child: Text(
                                                        h,
                                                        style: TextStyle(
                                                        fontSize: 14,
                                                        color: Colors.grey[800],
                                                      ),
                                                    ),
                                                  ))
                                              .toList(),
                                        ],
                                      );
                                    },
                                  ),
                              ),
                        ),
                      ],
                    ),
                  ),
                )
              ],
            ),
          )
        ],
      ),
    );
  }
}

class ForwardingPath {
  final String from, to;
  ForwardingPath({required this.from, required this.to});
}

class SimplePipelineDiagram extends StatelessWidget {
  final Map<String, String> ifStage, idStage, exStage, memStage, wbStage;
  final Map<String, dynamic> ifIdBuffer, idExBuffer, exMemBuffer, memWbBuffer;

  const SimplePipelineDiagram(
      {Key? key,
      required this.ifStage,
      required this.idStage,
      required this.exStage,
      required this.memStage,
      required this.wbStage,
      required this.ifIdBuffer,
      required this.idExBuffer,
      required this.exMemBuffer,
      required this.memWbBuffer})
      : super(key: key);

  @override
  Widget build(BuildContext ctx) {
    return CustomPaint(
      size: const Size(1200, 360),
      painter: SimplePipelinePainter(
          ifStage: ifStage,
          idStage: idStage,
          exStage: exStage,
          memStage: memStage,
          wbStage: wbStage,
          ifIdBuffer: ifIdBuffer,
          idExBuffer: idExBuffer,
          exMemBuffer: exMemBuffer,
          memWbBuffer: memWbBuffer),
    );
  }
}

class SimplePipelinePainter extends CustomPainter {
  final Map<String, String> ifStage, idStage, exStage, memStage, wbStage;
  final Map<String, dynamic> ifIdBuffer, idExBuffer, exMemBuffer, memWbBuffer;

  SimplePipelinePainter({
    required this.ifStage,
    required this.idStage,
    required this.exStage,
    required this.memStage,
    required this.wbStage,
    required this.ifIdBuffer,
    required this.idExBuffer,
    required this.exMemBuffer,
    required this.memWbBuffer,
  });

  @override
  void paint(Canvas c, Size s) {
    final double stageW = 180, stageH = 180, bufW = 180;
    final double gap = 60;
    final double startX = 80;
    final double topY = 20;
    final double bottomY = topY + 220;

    final Paint stageFill = Paint()..color = Colors.grey.shade400;
    final Paint stageBorder = Paint()
      ..color = Colors.grey.shade700
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;
    final Paint bufFill = Paint()
      ..color = Colors.blue.shade900.withOpacity(0.6);
    final Paint bufBorder = Paint()
      ..color = Colors.grey.shade700
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;

    final titleStyle = TextStyle(
      color: Colors.black,
      fontSize: 16,
      fontWeight: FontWeight.bold,
      fontFamily: 'SpaceMono',
    );
    final textStyle = TextStyle(
      color: Colors.black87,
      fontSize: 14,
      fontFamily: 'SpaceMono',
    );
    final instrStyle = TextStyle(
      color: Colors.blue.shade900,
      fontSize: 14,
      fontWeight: FontWeight.bold,
      fontFamily: 'SpaceMono',
    );

    final bufXs = [
      startX - 40 + stageW,
      startX - 40 + 2 * stageW + gap,
      startX - 40 + 3 * stageW + 2 * gap,
      startX - 40 + 4 * stageW + 3 * gap,
    ];
    final stageXs = [
      25 + startX,
      25 + startX + stageW + gap,
      25 + startX + 2 * stageW + 2 * gap,
      25 + startX + 3 * stageW + 3 * gap,
      25 + startX + 4 * stageW + 4 * gap,
    ];

    // Draw buffers (no meshing lines)
    final bufLabels = ['IF/ID', 'ID/EX', 'EX/MEM', 'MEM/WB'];
    final bufData = [ifIdBuffer, idExBuffer, exMemBuffer, memWbBuffer];
    for (int i = 0; i < 4; i++) {
      _drawBuffer(c, bufLabels[i], bufXs[i], topY, bufW, stageH, bufFill,
          bufBorder, titleStyle);
      _drawBufferContent(
          c, bufData[i], bufXs[i], topY, bufW, stageH, textStyle);
    }

    // Draw stages
    final stageLabels = ['FETCH', 'DECODE', 'EXECUTE', 'MEMORY', 'WRITEBACK'];
    final stageData = [ifStage, idStage, exStage, memStage, wbStage];
    for (int i = 0; i < 5; i++) {
      final instruction = stageData[i]['instruction'] ?? '';
      final stageColor = instructionColors[instruction];

      _drawStage(c, stageLabels[i], stageXs[i], bottomY, stageW, stageH,
          stageFill, stageBorder, titleStyle, stageColor);
      _drawStageContent(c, stageData[i], stageXs[i], bottomY, stageW, stageH,
          textStyle, instrStyle);
    }
  }

  void _drawStage(Canvas c, String lbl, double x, double y, double w, double h,
      Paint fill, Paint border, TextStyle ts, Color? stageColor) {
    final r = Rect.fromLTWH(x, y, w, h);
    if (stageColor != null) {
      final stageFill = Paint()..color = stageColor;
      c.drawRect(r, stageFill);
    } else {
      c.drawRect(r, fill);
    }
    c.drawRect(r, border);
    _drawText(c, lbl, x + w / 2, y + 16, ts, TextAlign.center);
  }

  void _drawBuffer(Canvas c, String lbl, double x, double y, double w, double h,
      Paint fill, Paint border, TextStyle ts) {
    final r = Rect.fromLTWH(x, y, w, h);
    c.drawRect(r, fill);
    c.drawRect(r, border);
    _drawText(c, lbl, x + w / 2, y + 16, ts, TextAlign.center);
    // meshing lines removed
  }

  void _drawText(
      Canvas c, String t, double x, double y, TextStyle s, TextAlign a) {
    final span = TextSpan(text: t, style: s);
    final tp =
        TextPainter(text: span, textAlign: a, textDirection: TextDirection.ltr);
    tp.layout();
    final dx = a == TextAlign.left
        ? x
        : (a == TextAlign.right ? x - tp.width : x - tp.width / 2);
    tp.paint(c, Offset(dx, y - tp.height / 2));
  }

  void _drawStageContent(Canvas c, Map<String, String> st, double x, double y,
      double w, double h, TextStyle ts, TextStyle istyle) {
    if (st['instruction']!.isNotEmpty) {
      _drawText(c, 'Instr: ${st['instruction']!}', x + w / 2, y + 40, istyle,
          TextAlign.center);
    }
    int line = 0;
    st.forEach((k, v) {
      if (k == 'instruction' || v.isEmpty) return;
      _drawText(
          c, '$k: $v', x + w / 2, y + 66 + line * 20, ts, TextAlign.center);
      line++;
    });
  }

  void _drawBufferContent(Canvas c, Map<String, dynamic> buf, double x,
      double y, double w, double h, TextStyle ts) {
    int line = 0;
    buf.forEach((k, v) {
      if (v == null || v.toString().isEmpty) return;
      final txt = v.toString().length > 8
          ? '${v.toString().substring(0, 6)}...'
          : v.toString();
      _drawText(
          c, '$k: $txt', x + w / 2, y + 50 + line * 20, ts, TextAlign.center);
      line++;
      if (line >= 3) return;
    });
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}
