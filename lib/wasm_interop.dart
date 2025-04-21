import 'dart:async';
import 'dart:js' as js;
import 'dart:js_util' as js_util;

/// Common interface for both simulator variants.
abstract class Simulator {
  Future<void> init();
  String assemble(String code);
  void cleanup();
  void loadCode(String code);
  bool step();
  void run();
  void reset();
  String showReg();
  String showMem(String segment, int startAddr, int count);
  String getPC();
  String getConsoleOutput();
  void clearConsoleOutput();
  String getStats();
  String getBP();
  void toggleForwarding(bool enable);
  String getPipelineState();
  String getBuffers();
  void setPrintPipelineForInstruction(String pc);
}

/// Basic, non‑pipelined RISC‑V simulator.
class RiscVSimulator implements Simulator {
  late js.JsObject _simulator;

  RiscVSimulator._internal();

  static Future<RiscVSimulator> create() async {
    await _waitForModule();
    final ctor = js.context['Module']['RiscVSimulator'];
    if (ctor == null) throw Exception("RiscVSimulator ctor not found");
    var sim = RiscVSimulator._internal();
    sim._simulator = js.JsObject(ctor, []);
    return sim;
  }

  static Future<void> _waitForModule() async {
    while (js.context['Module'] == null ||
           !js.context['Module'].hasProperty('RiscVSimulator')) {
      await Future.delayed(Duration(milliseconds: 100));
    }
  }

  @override
  Future<void> init() async => _simulator.callMethod('init', []);
  @override
  String assemble(String code) => _simulator.callMethod('assemble', [code]);
  @override
  void cleanup() => _simulator.callMethod('cleanup', []);
  @override
  void loadCode(String code) => _simulator.callMethod('loadCode', [code]);
  @override
  bool step() => _simulator.callMethod('step', []);
  @override
  void run() => _simulator.callMethod('run', []);
  @override
  void reset() => _simulator.callMethod('reset', []);
  @override
  String showReg() => _simulator.callMethod('showReg', []);
  @override
  String showMem(String segment, int startAddr, int count) =>
      _simulator.callMethod('showMem', [segment, startAddr, count]);
  @override
  String getPC() => _simulator.callMethod('getPC', []);
  @override
  String getConsoleOutput() => _simulator.callMethod('getConsoleOutput', []);
  @override
  void clearConsoleOutput() => _simulator.callMethod('clearConsoleOutput', []);
  @override
  String getStats() => '';
  @override
  String getBP() => '';
  @override
  void toggleForwarding(bool enable) => {};
  @override
  String getPipelineState() => '';
  @override
  String getBuffers() => '';
  @override
  void setPrintPipelineForInstruction(String PC) => {};

}

/// Pipelined RISC‑V simulator with additional controls.
class RiscVPipelinedSimulator implements Simulator {
  late js.JsObject _simulator;

  RiscVPipelinedSimulator._internal();

  static Future<RiscVPipelinedSimulator> create() async {
    await RiscVSimulator._waitForModule();
    final ctor = js.context['PipelineModule']['RiscVPipelinedSimulator'];
    if (ctor == null) throw Exception("RiscVPipelinedSimulator ctor not found");
    var sim = RiscVPipelinedSimulator._internal();
    sim._simulator = js.JsObject(ctor, []);
    return sim;
  }

  @override
  Future<void> init() async => _simulator.callMethod('init', []);
  @override
  String assemble(String code) => _simulator.callMethod('assemble', [code]);
  @override
  void cleanup() => _simulator.callMethod('cleanup', []);
  @override
  void loadCode(String code) => _simulator.callMethod('loadCode', [code]);
  @override
  bool step() => _simulator.callMethod('step', []);
  @override
  void run() => _simulator.callMethod('run', []);
  @override
  void reset() => _simulator.callMethod('reset', []);
  @override
  String showReg() => _simulator.callMethod('showReg', []);
  @override
  String showMem(String segment, int startAddr, int count) =>
      _simulator.callMethod('showMem', [segment, startAddr, count]);
  @override
  String getPC() => _simulator.callMethod('getPC', []);
  @override
  String getConsoleOutput() => _simulator.callMethod('getConsoleOutput', []);
  @override
  void clearConsoleOutput() => _simulator.callMethod('clearConsoleOutput', []);
  @override
  String getStats() => _simulator.callMethod('getStats', []);
  @override
  void toggleForwarding(bool enable){ _simulator.callMethod('toggleForwarding', [enable]);}
  @override
  String getPipelineState() => _simulator.callMethod('getPipelineState', []);
  @override
  String getBP() => _simulator.callMethod('getBP', []);
  @override
  String getBuffers() => _simulator.callMethod('getBuffers', []);
  @override
  void setPrintPipelineForInstruction(String PC) =>
      _simulator.callMethod('setPrintPipelineForInstruction', [PC]);
}