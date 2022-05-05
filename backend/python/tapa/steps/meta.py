from typing import Dict
import click

import tapa.steps.analyze as analyze
import tapa.steps.synth as synth
import tapa.steps.optimize as optimize
import tapa.steps.link as link
import tapa.steps.pack as pack


@click.command()
@click.pass_context
@click.option('--floorplan / --no-floorplan',
              type=bool,
              default=False,
              help="Enable floorplanning for the design")
def compile(ctx: click.Context, floorplan: bool, **kwargs: Dict):
  
  forward_applicable(ctx, analyze.analyze, kwargs)
  forward_applicable(ctx, synth.synth, kwargs)
  if floorplan:
    forward_applicable(ctx, optimize.optimize_floorplan, kwargs)
  forward_applicable(ctx, link.link, kwargs)
  forward_applicable(ctx, pack.pack, kwargs)


compile.params.extend(analyze.analyze.params)
compile.params.extend(synth.synth.params)
compile.params.extend(optimize.optimize_floorplan.params)
compile.params.extend(link.link.params)
compile.params.extend(pack.pack.params)


def forward_applicable(ctx: click.Context, command: click.Command,
                       kwargs: Dict):
  names = list(map(lambda param: param.name, command.params))
  args = {key: value for (key, value) in kwargs.items() if key in names}
  ctx.invoke(command, **args)
