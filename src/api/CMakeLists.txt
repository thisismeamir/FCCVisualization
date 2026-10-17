# API layer (T5): builders, Script, interpreter, completion, configuration.
fccvis_add_module(api SOURCES Config.cc Expression.cc Interpreter.cc DEPENDS session stdlib adapters)
