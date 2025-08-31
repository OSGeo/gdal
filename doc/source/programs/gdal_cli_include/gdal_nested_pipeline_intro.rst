It is possible to create "nested pipelines", i.e. pipelines inside pipelines.

A nested pipeline is delimited by square brackets (``[`` and ``]``) surrounded
by a space character.

There are 2 kinds of nested pipelines:

* input nested pipelines: where the result dataset of the nested pipeline is
  used as the input dataset for an argument of the main pipeline.

* output nested pipelines: where the output of a step of the main pipeline is
  used as the input of the nested pipeline in a following step. Output nested
  pipelines can only be used with the ``tee`` step.
