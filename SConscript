# RT-Thread building script for bridge
from building import *

cwd = GetCurrentDir()

# 获取源文件
src = Glob('src/*.c')
src += Glob('util/*.c')

# 包含路径
CPPPATH = [cwd + '/inc', cwd + '/util']

# 检查是否定义了 USING_TM1668_DEMO 宏
if GetDepend('USING_TM1668_DEMO'):
    src += Glob('example/*.c')
    # 添加示例代码的头文件路径
    CPPPATH += [cwd + '/example']

# 创建组件组
group = DefineGroup('tm1668', src, depend=['PKG_USING_TM1668'], CPPPATH=CPPPATH)

Return('group')
