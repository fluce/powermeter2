class LineBreakTransformer {
  private container: string;

  constructor() {
    this.container = '';
  }

  transform(chunk: string, controller: TransformStreamDefaultController<string>) {
    this.container += chunk;
    const lines = this.container.split('\r\n');
    this.container = lines.pop() || '';
    lines.forEach(line => controller.enqueue(line));
  }

  flush(controller: TransformStreamDefaultController<string>) {
    if (this.container) {
      controller.enqueue(this.container);
    }
  }
}

export default LineBreakTransformer;
