#ifndef MUDUO_BASE_COPYABLE_H
#define MUDUO_BASE_COPYABLE_H

namespace muduo
{

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.
/// 一个空基类，用于标识值类型,即可拷贝的
class copyable
{
};

};

#endif  // MUDUO_BASE_COPYABLE_H
