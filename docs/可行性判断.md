# 《灰尾回收 / 五四三二一》Demo 数值分析文档：随机撤离权模型、探索风险与配置合理性

## 摘要

本文分析当前 Demo 配置下的扫雷式撤离探索模型。地图采用 (10\times10) 方格，起点固定且安全，其余 99 个格子随机生成 20 个雷房、10 个事件房、10 个宝箱房、10 个怪物房、1 个撤离点和 48 个普通房。玩家只能四向移动，但每进入一个房间可以看到其周围 8 邻域内的雷房数量。已探索区域可以免费通行，因此真实探索成本不应按物理步数计算，而应按进入未知房间的次数计算。

本文在此前最短路模型、随机游走模型和未知格访问序列模型的基础上，进一步修正了一个关键点：玩家很多时候必须先探索到撤离点才能成功撤退。撤离点不是普通奖励，而是“停止权”的随机解锁事件。由此，整局应被建模为两个阶段：撤离点未发现时的强制探索阶段，以及撤离点已发现后的可停止决策阶段。该修正会使当前 20 雷单撤离点配置的风险判断比原模型更严格。

核心结论是：当前 (10\times10)、20 雷、10 事件、10 宝箱、10 怪物、单撤离点配置仍可以作为 Demo 成立，但它不是宽容型随机探索配置，而是高压、高信息密度、强依赖扫雷数字理解的推理型配置。在必须先找到撤离点才能撤离、且压力 80 后每探索未知格扣 2 HP 的模型下，有效踩雷率目标应从此前的约 (3%\sim5%) 修正为约 (1%\sim3%)。若实际玩家无法通过 UI、标记、数字提示和起点引导达到该避雷效率，则 20 雷单撤离点配置会明显偏难。

## 1. 基础规则与建模对象

地图为

[
G={1,\dots,10}\times{1,\dots,10}.
]

共有 100 个格子。起点固定且安全，其余 99 个格子随机生成房间内容。当前 Demo 配置为：

[
20\text{ 雷房},\quad10\text{ 事件房},\quad10\text{ 宝箱房},\quad10\text{ 怪物房},\quad1\text{ 撤离点},\quad48\text{ 普通房}.
]

房间类型不重叠。玩家移动采用四邻接规则，即只能从 ((x,y)) 移动到满足

[
|x-x'|+|y-y'|=1
]

的相邻房间。雷数显示采用 8 邻域规则。若玩家进入非雷房 (v)，则显示

[
C(v)=\sum_{u\in N_8(v)}\mathbf 1_{{u\text{ 为雷房}}},
]

其中 (N_8(v)) 是 (v) 周围八个方向内的格子集合。没有传统扫雷中的空白格展开规则。即使数字为 0，也只表示该房间周围 8 格无雷，不会自动揭示这些格子的后续数字。

当前数值参数为：

[
HP_0=100,
]

[
MineDamage=30,
]

[
PressureGain_{\text{unknown}}=2,
]

[
PressureGain_{\text{mine}}=10,
]

[
PressureGain_{\text{monster}}=5.
]

协议 1 区间为压力 (80\sim100)。本文采用新增规则：

[
\text{若进入未知格前 Pressure}\ge80,\text{ 本次探索额外 }-2HP.
]

失败时待结算币丢失，背包物资只能带出 1 件；成功撤离时待结算币转入全局，物资全部带回。

## 2. 已知终点的最短路径基准

若起点和终点均已知，四向移动下的最短距离为曼哈顿距离：

[
d((r_1,c_1),(r_2,c_2))=|r_1-r_2|+|c_1-c_2|.
]

在 (10\times10) 方格中，若两个格子独立均匀抽取且允许相同，则一维坐标差期望为：

[
\mathbb E|X-Y|
==============

\frac{1}{100}\sum_{i=1}^{10}\sum_{j=1}^{10}|i-j|.
]

按差值 (d) 分组，差值为 (d) 的有序坐标对数量为 (2(10-d))，所以：

[
\sum_{i=1}^{10}\sum_{j=1}^{10}|i-j|
===================================

# 2\sum_{d=1}^{9}d(10-d)

330.

]

因此一维期望为：

[
3.3.
]

二维曼哈顿距离期望为：

[
6.6.
]

若要求起点和终点不同，则相同格子概率为 (1/100)，且相同格子的距离为 0，因此条件期望为：

[
\mathbb E[d\mid \text{不同}]
==========================

# \frac{6.6}{99/100}

\frac{20}{3}.
]

该结果只表示“已知目标且无风险时的最短路径下界”，不能直接表示真实探索成本。真实游戏中，撤离点未知、风险未知、玩家必须根据数字信息逐步探索。

## 3. 随机游走基准

若玩家不知道终点，也不使用信息，而是在每一步从当前房间的合法四向邻格中等概率选择移动方向，则过程是 (10\times10) 方格图上的简单随机游走。设终点为 (t)，从格子 (i) 首次到达 (t) 的期望步数为 (H_i^{(t)})。则：

[
H_t^{(t)}=0,
]

且对 (i\ne t)：

[
H_i^{(t)}
=========

1+\frac1{\deg(i)}\sum_{j\sim i}H_j^{(t)}.
]

对所有不同起点终点平均，数值解约为：

[
\mathbb E_{\text{rw}}\approx244.77.
]

该模型提供的是“完全无策略、重复在已知区域内随机移动”的高成本参考。但当前游戏允许已探索区域免费通行，所以随机游走模型会显著高估真实探索成本。

## 4. 已探索区域免费通行与未知格访问序列

当前规则中，所有已探索房间均视为可行路径，玩家可以在已探索连通区域内免费移动。因此，真正的探索成本不是物理移动步数，而是进入未知房间的次数。

设起点固定后，其余 (99) 个格子被探索过程排成某个访问序列：

[
v_1,v_2,\dots,v_{99}.
]

若撤离点在这 99 个格子中均匀随机，并且访问顺序在发现撤离点之前不依赖撤离点本身，则在无死亡、无额外撤离情报的基准模型中，撤离点名次 (T) 满足：

[
T\sim Uniform(1,99).
]

因此：

[
\mathbb E[T]=\frac{1+99}{2}=50.
]

这个结论在“无死亡基准模型”中成立，但不能直接等同于“真实玩家发现撤离点时的平均状态”。原因是玩家可能在发现撤离点前死亡，因此真正重要的是条件分布：

[
S_T\mid \text{alive at exit discovery}.
]

晚发现撤离点的局更容易因雷伤和压力扣血失败，所以活着发现撤离点的样本会偏向更早发现、更少踩雷、更高 HP 和更低压力。

## 5. 撤离点作为随机解锁停止权

撤离点不是普通收益格，而是玩家成功撤离权的解锁事件。发现撤离点前，玩家无法主动成功撤离；发现撤离点后，玩家可以随时选择撤离。因此整局必须拆成两个阶段。

设状态为：

[
S=(\mathcal I,HP,P,V,B,E),
]

其中 (\mathcal I) 为玩家已知信息，(HP) 为生命值，(P) 为压力，(V) 为当前若成功撤离可带出价值，(B) 为失败保底价值，(E\in{0,1}) 表示撤离权是否已解锁。

若 (E=0)，撤离点未发现，玩家没有立即撤离选项：

[
F_0(S)=\max_{a\in A(S)}Q_0(S,a).
]

若 (E=1)，撤离点已发现，玩家拥有停止权：

[
F_1(S)=\max\left(V,\max_{a\in A(S)}Q_1(S,a)\right).
]

这一区分是当前模型的核心修正。原先的边际公式：

[
\Delta=(1-f)Y-f(V-B)
]

只适用于 (E=1) 的阶段，即“已经可以撤离，但选择是否继续探索”。它不能直接描述 (E=0) 时的强制探索阶段。

当 (E=0) 时，探索一个候选格 (a) 的价值中应包含“发现撤离点”的选择权价值。设：

[
\lambda_a=P(a=\text{Exit}\mid \mathcal I),
]

则可写成：

[
Q_0(S,a)
========

(1-d_a)\left[Y_a+\lambda_aO(S')\right]-d_a(V-B),
]

其中 (d_a) 为探索 (a) 导致失败的概率，(Y_a) 为进入该格且存活时的收益，(O(S')) 为发现撤离点后获得停止权的价值。

当 (E=1) 时，停止权已经拥有，不再有 (\lambda_aO(S')) 项：

[
Q_1(S,a)=(1-d_a)Y_a-d_a(V-B).
]

因此，同样一个未知格，在撤离点未发现时的探索价值高于撤离点已发现后。玩家未发现撤离点时愿意承受更高风险，不只是因为心理焦虑，而是因为继续探索包含“获得撤离权”的期望价值。

## 6. 撤离点未发现阶段的强制风险

现在最关键的指标不再是“发现撤离点后是否继续探索”，而是：

[
P(\text{alive at exit discovery}).
]

令：

[
q=\text{玩家实际有效踩雷率},
]

[
m=\text{怪物遭遇并处理率}.
]

每探索一个未知格的平均压力增长近似为：

[
\alpha=2+10q+5m.
]

协议 1 触发时间近似为：

[
\tau=\frac{80}{\alpha}.
]

在撤离点名次为 (T) 时，期望生命损失可以近似为：

[
L(T)=30qT+2\max(0,T-\tau).
]

第一项是雷伤期望，第二项是压力达到 80 后每探索未知格扣 2 HP 的期望损失。该式忽略了治疗、减压、怪物直接伤害、事件负收益等因素，因此是一个一阶近似模型。

玩家能活着发现撤离点的近似条件是：

[
L(T)<100.
]

令临界撤离点名次 (T_c) 满足：

[
30qT_c+2(T_c-\tau)=100.
]

解得：

[
T_c=\frac{100+2\tau}{30q+2}.
]

由于无死亡基准下：

[
T\sim Uniform(1,99),
]

所以活着发现撤离点的粗略概率为：

[
P(\text{alive at exit discovery})
\approx
\frac{T_c}{99},
]

并应截断在 (0) 与 (1) 之间。

该公式说明：撤离点未发现阶段的风险由三个因素共同决定，即有效踩雷率 (q)、怪物压力贡献 (m)、撤离点名次 (T)。原模型中只看 (T=50) 的平均发现时间，会低估“撤离点过晚”带来的尾部失败风险。

## 7. 当前 Demo 参数下的分情况推导

当前怪物房数量为 10。若玩家主要探索非雷格，怪物遭遇率可近似取：

[
m\approx\frac{10}{79}\approx12.7%.
]

若按整体 99 格粗略看，也可取：

[
m\approx\frac{10}{99}\approx10.1%.
]

因此本文在估算中取中间区间：

[
m\approx0.10\sim0.13.
]

为便于计算，取：

[
m=0.12.
]

当有效踩雷率为 (q) 时：

[
\alpha=2+10q+5m.
]

于是：

[
\tau=\frac{80}{2+10q+5m}.
]

不同 (q) 下得到：

[
q=1%:\quad \alpha=2.70,\quad \tau\approx29.6,
]

[
q=2%:\quad \alpha=2.80,\quad \tau\approx28.6,
]

[
q=3%:\quad \alpha=2.90,\quad \tau\approx27.6,
]

[
q=5%:\quad \alpha=3.10,\quad \tau\approx25.8.
]

再代入：

[
T_c=\frac{100+2\tau}{30q+2},
]

得到：

[
q=1%:\quad T_c\approx69.2,
]

[
q=2%:\quad T_c\approx60.4,
]

[
q=3%:\quad T_c\approx53.5,
]

[
q=5%:\quad T_c\approx43.3.
]

对应活着发现撤离点的粗略概率：

[
q=1%:\quad \frac{69.2}{99}\approx69.9%,
]

[
q=2%:\quad \frac{60.4}{99}\approx61.1%,
]

[
q=3%:\quad \frac{53.5}{99}\approx54.0%,
]

[
q=5%:\quad \frac{43.3}{99}\approx43.7%.
]

这说明，在必须先发现撤离点才能撤离的规则下，原先 (3%\sim5%) 的有效踩雷率目标偏乐观。若希望玩家较稳定地活着发现撤离点，有效踩雷率应更接近：

[
1%\sim3%.
]

其中 (1%) 较舒适，(2%) 有压力但可控，(3%) 接近高压边界，(5%) 在单撤离点模型下偏难。

## 8. 早发现、中段发现与过晚发现

按照撤离点名次 (T) 可分为三种情况。

若：

[
T\le\tau,
]

玩家在协议 1 额外扣血前发现撤离点。这是早发现状态。此时玩家较早获得停止权，后续主要进入“继续贪还是撤离”的标准边际决策。该状态的近似概率为：

[
\frac{\tau}{99}.
]

若取 (q=2%,m=0.12)，则：

[
\frac{28.6}{99}\approx28.9%.
]

若：

[
\tau<T<T_c,
]

玩家在中段发现撤离点。此时已经进入协议 1，并承受了一部分压力扣血，但尚未达到平均死亡阈值。这是最符合设计目标的区间，因为玩家已经积累了收益、压力和损伤，同时获得了撤离权，继续探索与撤离之间会形成明显张力。取 (q=2%,m=0.12) 时，该区间概率约为：

[
\frac{60.4-28.6}{99}\approx32.1%.
]

若：

[
T\ge T_c,
]

则玩家在平均意义上会在找到撤离点前失败。这是单撤离点模型的风险尾部。取 (q=2%,m=0.12) 时，该概率约为：

[
1-\frac{60.4}{99}\approx38.9%.
]

该结果说明，单撤离点配置的主要死亡来源并不一定是“发现撤离点后贪心过度”，而可能是“撤离点尚未发现时被迫继续探索”。这正是修正模型后得到的关键变化。

## 9. 内容曝光的修正

无死亡基准下，某类内容房间数量为 (c)，单撤离点时，在撤离前的期望出现数量为：

[
\frac c2.
]

当前事件、宝箱、怪物各 10 个，因此无死亡基准下各自期望出现：

[
5
]

个。

但真实体验中，玩家可能在发现撤离点前死亡，因此实际应计算：

[
\mathbb E[\text{内容数量}\mid \text{alive at exit discovery}].
]

这个值不一定等于 5。若死亡主要集中在撤离点过晚的局，活着发现撤离点的样本会偏早，因此内容曝光数量可能低于 5。另一方面，若事件、宝箱或怪物能够带来治疗、收益或战斗成长，它们也可能间接提高存活概率，造成条件分布偏差。

因此，“每类内容期望出现 5 个”应被保留为：

[
\boxed{\text{无死亡内容曝光基准}}
]

而不是完整真实体验值。

若未来有 (E) 个撤离点，则无死亡基准下：

[
\mathbb E[\text{某类内容房撤离前出现数}]
=============================

\frac{c}{E+1}.
]

该类内容至少出现一次的概率为：

[
\frac{c}{c+E}.
]

当 (E=1,c=10) 时，退化为：

[
\frac{10}{2}=5,
]

以及：

[
\frac{10}{11}\approx90.9%.
]

## 10. 盲走模型的重新解释

若玩家盲走，只考虑雷伤而忽略压力扣血，则撤离点前雷数 (R) 在 (0,\dots,20) 上近似均匀。玩家第 4 次踩雷失败，因此：

[
P(\text{只因雷伤而活着发现撤离点})
======================

# P(R\le3)

\frac4{21}
\approx19.05%.
]

但现在加入了压力扣血，盲走玩家即使撤离点前雷数不超过 3，也可能因为撤离点过晚而因压力扣血失败。因此：

[
\frac4{21}
]

不再是完整盲走生存率，而是：

[
\boxed{\text{只考虑雷伤时的盲走存活上界}}
]

完整盲走存活率应低于该值。这进一步强化了结论：当前配置并不服务于随机乱走玩家，必须依赖扫雷信息。

## 11. 发现撤离点后的边际模型

一旦撤离点已发现，玩家拥有随时撤离权。此时继续探索一个未知格相对于立即撤离的边际期望为：

[
\Delta=(1-f)Y-f(V-B).
]

其中 (Y) 是存活时新增收益，(f) 是该次探索导致失败的概率，(V) 是当前成功撤离价值，(B) 是失败保底价值。

如果采用均值近似：

[
V=K\mu,
]

[
B=rV,
]

[
Y=\mu,
]

则平衡条件为：

[
(1-f)\mu=f(1-r)K\mu.
]

解得：

[
f^*=\frac{1}{1+(1-r)K}.
]

该模型仍然正确，但现在必须明确它只适用于撤离点已发现阶段。对于撤离点未发现阶段，必须使用包含撤离权价值的 (Q_0(S,a))。

此外，当前 (B=rV) 只是均值近似。真实失败保底应写成状态函数：

[
B=B(S)=\operatorname{BestKeepValue}(\text{背包物资},\text{保底规则}).
]

因为失败时保留的是 1 件物资，而不是固定比例价值。若玩家身上有一个极高价值物品，(B/V) 会偏高；若物品价值分散，(B/V) 会偏低。

## 12. 压力扣血与 (HP>32) 结论的修正

协议 1 下探索未知格的额外扣血为 2，踩雷伤害为 30，所以若只考虑“压力扣血 + 雷伤”，单格最大即时伤害为：

[
32.
]

因此，在只考虑雷伤与协议扣血的一格保守模型中，若：

[
HP>32,
]

则下一格不会因为“压力扣血 + 雷伤”直接导致死亡。此时若撤离点已发现，玩家可以采取“探索一格，若存活则立刻撤离”的策略。

但该结论必须限定适用范围。它不考虑怪物直接伤害、事件负收益、连续探索决策、背包满、负面藏品等因素。因此它不能写成完整游戏中的一般结论，只能写成：

[
\boxed{
在只考虑雷伤与协议扣血的一格模型中，(HP>32) 意味着下一格无即时死亡风险。
}
]

这个结论仍然有用，因为它说明压力扣血主要把低血高压玩家推入撤离决策区，而不是在早期强行劝退所有玩家。

## 13. 玩家失误率临界的修正

此前基于后撤离点边际模型，曾取：

[
q^*\approx3.7%
]

并估计理性玩家有效踩雷率：

[
q_{\text{opt}}\approx1.0%.
]

原始雷率为：

[
q_{\text{raw}}\approx20.2%.
]

若玩家有 (\varepsilon) 的概率失误，失误时近似随机选择候选格，则：

[
q(\varepsilon)
==============

(1-\varepsilon)q_{\text{opt}}+\varepsilon q_{\text{raw}}.
]

临界失误率为：

[
\varepsilon^*
=============

\frac{q^*-q_{\text{opt}}}{q_{\text{raw}}-q_{\text{opt}}}.
]

原先若取 (q^*=3.7%)，得到：

[
\varepsilon^*\approx14.1%.
]

但在修正后的强制撤离点发现模型中，临界有效踩雷率应更保守。若取：

[
q^*=2.5%,
]

则：

[
\varepsilon^*
=============

\frac{2.5%-1.0%}{20.2%-1.0%}
\approx7.8%.
]

因此，当前 Demo 的玩家失误率临界应从此前的约 (14%\sim16%) 修正为约：

[
8%\sim10%.
]

也就是，若玩家大约每 10 到 12 次关键判断中失误 1 次，就可能接近高压临界。这个结论仍然是设计估算，不是完整 POMDP 的严格解。它依赖 (q_{\text{opt}}) 的程序估计和 (q^*) 的设计选取。

## 14. 普通房小宝箱与宝箱房收益关系

普通房可以有低价值小收益，但不能使用完整宝箱房掉落表。若普通搜索物资价值期望约为：

[
0.35\cdot5.5+0.08\cdot17.5+0.02\cdot70=4.725,
]

再加上普通搜索待结算币约 (1.5\sim1.6)，则普通小宝箱总期望约为：

[
6.3.
]

宝箱房若按主掉落表估算，物资期望约为：

[
0.15\cdot5.5
+
0.45\cdot17.5
+
0.30\cdot70
+
0.09\cdot275
+
0.01\cdot1900
=============

73.45.
]

再加宝箱待结算币约：

[
6.6,
]

宝箱房总期望约为：

[
80.05.
]

因此宝箱房单房间期望约为普通小宝箱的：

[
\frac{80.05}{6.3}\approx12.7
]

倍。

该结论应理解为单房间收益比，而不是一局总收益贡献比。一局中普通房和宝箱房实际贡献还取决于玩家是否活着发现撤离点、发现前遇到多少房间、发现后是否继续探索。只要普通房使用低价值收益表，该设计仍合理；若普通房使用完整宝箱房掉落表，则会稀释宝箱房价值，不合理。

## 15. 当前 Demo 配置的最终判断

当前配置为：

[
10\times10,\quad20\text{ 雷},\quad10\text{ 事件},\quad10\text{ 宝箱},\quad10\text{ 怪物},\quad1\text{ 撤离点}.
]

在旧模型中，重点是“发现撤离点后继续探索是否值得”。在修正模型中，重点应前移为：

[
\boxed{
玩家能否活着发现撤离点。
}
]

修正后的结论是：

[
\boxed{
当前配置仍可成立，但成立条件比原模型更严格。
}
]

它不是宽容型随机探索配置，而是高压推理型 Demo。其成立前提是玩家能够把实际有效踩雷率压到约：

[
1%\sim3%.
]

如果实际有效踩雷率接近 (5%) 或更高，则在单撤离点、压力扣血开启的规则下，玩家会在找到撤离点前承受过高强制探索风险。

因此，当前配置可以保留，但应至少配套一种撤离前缓冲机制。这些机制包括清晰数字 UI、标记功能、数字影响范围高亮、起点安全保护、初始扫描针、首次踩雷减伤、撤离方向提示、减压或回血道具，或增加第二撤离点。

其中，对数学模型影响最直接的是增加撤离点数量。若撤离点由 1 个改为 2 个，则无死亡基准下首次发现撤离点的期望名次从：

[
50
]

降为：

[
\frac{100}{3}\approx33.3.
]

这会显著削弱“找到撤离点前的强制探索风险”。

## 16. 结论

本文修正后的核心结论如下。

第一，起点固定安全、其余 99 格随机生成是当前分析的统一基准。此前程序中的边角保护应从基准模型中移除，除非正式规则明确采用起点保护。

第二，撤离点必须被视为停止权的随机解锁事件。整局应分为撤离点未发现阶段与撤离点已发现阶段。前者是强制探索问题，后者才是继续探索与撤离的边际收益问题。

第三，原先的 (T=50) 只是无死亡基准下的撤离点平均名次，不能直接代表真实玩家发现撤离点时的状态。真实模型应分析：

[
S_T\mid \text{alive at exit discovery}.
]

第四，压力 80 后每探索未知格扣 2 HP 的规则，在单撤离点模型中显著提高撤离点未发现阶段的风险。加入怪物压力和有效踩雷率后，协议 1 通常会早于第 40 格触发。

第五，当前 20 雷配置仍然可以作为 Demo 成立，但其合理有效踩雷率目标应修正为约 (1%\sim3%)，而不是原先偏宽的 (3%\sim5%)。玩家失误率临界也应从约 (14%\sim16%) 下修到约 (8%\sim10%)。

第六，10 事件、10 宝箱、10 怪物仍然有利于 Demo 内容曝光，但“撤离前每类期望出现 5 个”只能作为无死亡基准，而不是完整真实体验值。

第七，普通房小宝箱合理，但必须保持低价值定位；宝箱房仍应作为高价值物资入口，二者单房间期望最好维持数量级差异。

最终，当前 Demo 的准确定位应为：

[
\boxed{
高信息密度、高压、强依赖扫雷数字理解的撤离推理 Demo。
}
]

它可以成立，但必须通过 UI、教学、标记、起点保护或撤离前缓冲机制，确保玩家体验不是盲走模型，而是真正进入“读数字、判风险、找撤离、决定是否继续贪”的核心循环。

# 附录 A：随机 99 格基准蒙特卡洛程序

本附录给出一个与正文一致的基准模拟程序。程序采用起点固定安全、其余 99 格随机生成的模型，不默认起点四向保护。它用于比较盲走玩家、低风险玩家和全知避雷玩家在当前 Demo 配置下的表现。

该程序不是完整 POMDP 求解器。`minrisk` 策略代表保守低风险玩家，而不是完整非全知绝对理性玩家。完整理性玩家还会考虑收益、信息价值、撤离权价值和失败保底。该程序主要用于验证参数趋势和估算量级。

```python
from random import Random
from math import comb
import statistics

N = 10

MINE = "mine"
EVENT = "event"
CHEST = "chest"
MONSTER = "monster"
EXIT = "exit"
NORMAL = "normal"
START = "start"


def neighbors4(cell, n=N):
    x, y = cell
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < n and 0 <= ny < n:
            yield (nx, ny)


def neighbors8(cell, n=N):
    x, y = cell
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < n:
                yield (nx, ny)


def generate_map(
    rng,
    n=10,
    mines=20,
    events=10,
    chests=10,
    monsters=10,
    start=(0, 0),
):
    """
    正文基准模型：
    起点固定且安全；
    除起点外的 99 个格子随机生成全部内容；
    不进行起点四向保护。
    """
    cells = [(i, j) for i in range(n) for j in range(n)]
    available = [c for c in cells if c != start]
    rng.shuffle(available)

    room = {c: NORMAL for c in cells}
    room[start] = START

    def take(k, room_type):
        nonlocal available
        chosen = available[:k]
        available = available[k:]
        for c in chosen:
            room[c] = room_type

    take(mines, MINE)
    take(events, EVENT)
    take(chests, CHEST)
    take(monsters, MONSTER)
    take(1, EXIT)

    return room


def clue(room, cell, n=10):
    return sum(1 for u in neighbors8(cell, n) if room[u] == MINE)


def get_frontier(explored, n=10):
    frontier = set()
    for c in explored:
        for u in neighbors4(c, n):
            if u not in explored:
                frontier.add(u)
    return list(frontier)


def reward_for_room(room, cell, rng):
    room_type = room[cell]

    if room_type in (NORMAL, START):
        k = clue(room, cell)
        coins = min(4, rng.randint(0, 2) + k // 2)
        r = rng.random()
        items = []

        if r < 0.35:
            items.append(rng.uniform(3, 8))
        elif r < 0.43:
            items.append(rng.uniform(10, 25))
        elif r < 0.45:
            items.append(rng.uniform(40, 100))

        return coins, items

    if room_type == CHEST:
        k = clue(room, cell)
        coins = min(11, rng.randint(3, 7) + k)
        r = rng.random()

        if r < 0.15:
            item = rng.uniform(3, 8)
        elif r < 0.60:
            item = rng.uniform(10, 25)
        elif r < 0.90:
            item = rng.uniform(40, 100)
        elif r < 0.99:
            item = rng.uniform(150, 400)
        else:
            item = rng.uniform(800, 3000)

        items = [item]

        if rng.random() < 0.25:
            items.append(rng.uniform(3, 25))

        return coins, items

    if room_type == MONSTER:
        coins = rng.randint(0, 3)
        items = []

        if rng.random() < 0.15:
            items.append(rng.uniform(3, 25))

        return coins, items

    return 0, []


def fallback_risk_estimate(room, explored, n=10, total_mines=20):
    cells = [(i, j) for i in range(n) for j in range(n)]
    unknown = [c for c in cells if c not in explored]

    known_mines = sum(1 for c in explored if room[c] == MINE)
    remaining_mines = total_mines - known_mines

    base = remaining_mines / max(1, len(unknown))
    risk = {c: base for c in unknown}

    for v in explored:
        if room[v] == MINE:
            continue

        unknown_neighbors = [u for u in neighbors8(v, n) if u not in explored]
        if not unknown_neighbors:
            continue

        known_neighbor_mines = sum(
            1 for u in neighbors8(v, n)
            if u in explored and room[u] == MINE
        )

        remaining = clue(room, v, n) - known_neighbor_mines

        if remaining <= 0:
            for u in unknown_neighbors:
                risk[u] = 0.0
        elif remaining >= len(unknown_neighbors):
            for u in unknown_neighbors:
                risk[u] = 1.0
        else:
            local = remaining / len(unknown_neighbors)
            for u in unknown_neighbors:
                if risk[u] not in (0.0, 1.0):
                    risk[u] = max(risk[u], local)

    return risk


def posterior_probs(room, explored, n=10, total_mines=20, max_enum_vars=20):
    """
    对未知格雷率进行局部估计。
    若前沿变量数不超过 max_enum_vars，则枚举局部约束；
    否则退化为启发式估计。
    """
    cells = [(i, j) for i in range(n) for j in range(n)]
    unknown = [c for c in cells if c not in explored]

    known_mines = sum(1 for c in explored if room[c] == MINE)
    remaining_total_mines = total_mines - known_mines

    constraints = []
    variable_set = set()

    for v in explored:
        if room[v] == MINE:
            continue

        unknown_neighbors = [u for u in neighbors8(v, n) if u not in explored]

        if not unknown_neighbors:
            continue

        known_neighbor_mines = sum(
            1 for u in neighbors8(v, n)
            if u in explored and room[u] == MINE
        )

        target = clue(room, v, n) - known_neighbor_mines

        if target < 0 or target > len(unknown_neighbors):
            return fallback_risk_estimate(room, explored, n, total_mines)

        constraints.append((unknown_neighbors, target))
        variable_set.update(unknown_neighbors)

    if not constraints or len(variable_set) > max_enum_vars:
        return fallback_risk_estimate(room, explored, n, total_mines)

    variables = list(variable_set)
    index = {c: i for i, c in enumerate(variables)}

    constraint_vars = []
    targets = []

    for unknown_neighbors, target in constraints:
        constraint_vars.append([index[u] for u in unknown_neighbors])
        targets.append(target)

    m = len(variables)
    outside_count = len(unknown) - m

    contained_in = [[] for _ in range(m)]

    for ci, vs in enumerate(constraint_vars):
        for vi in vs:
            contained_in[vi].append(ci)

    current_counts = [0] * len(constraint_vars)
    unassigned_counts = [len(vs) for vs in constraint_vars]

    marginals = [0.0] * m
    denominator = 0.0
    outside_probability_numerator = 0.0

    order = sorted(range(m), key=lambda i: -len(contained_in[i]))
    assignment = [None] * m

    def dfs(pos, local_mines):
        nonlocal denominator, outside_probability_numerator

        if pos == m:
            if any(current_counts[i] != targets[i] for i in range(len(targets))):
                return

            remaining_outside = remaining_total_mines - local_mines

            if remaining_outside < 0 or remaining_outside > outside_count:
                return

            weight = comb(outside_count, remaining_outside)

            if weight == 0:
                return

            denominator += weight

            for i, value in enumerate(assignment):
                if value == 1:
                    marginals[i] += weight

            if outside_count > 0:
                outside_probability_numerator += weight * (
                    remaining_outside / outside_count
                )

            return

        vi = order[pos]

        for value in (0, 1):
            assignment[vi] = value
            ok = True

            for ci in contained_in[vi]:
                unassigned_counts[ci] -= 1
                current_counts[ci] += value

                if current_counts[ci] > targets[ci]:
                    ok = False

                if current_counts[ci] + unassigned_counts[ci] < targets[ci]:
                    ok = False

            if ok and local_mines + value <= remaining_total_mines:
                dfs(pos + 1, local_mines + value)

            for ci in contained_in[vi]:
                current_counts[ci] -= value
                unassigned_counts[ci] += 1

            assignment[vi] = None

    dfs(0, 0)

    if denominator == 0:
        return fallback_risk_estimate(room, explored, n, total_mines)

    outside_probability = (
        outside_probability_numerator / denominator
        if outside_count > 0
        else 0.0
    )

    risk = {}

    for c in unknown:
        if c in index:
            risk[c] = marginals[index[c]] / denominator
        else:
            risk[c] = outside_probability

    return risk


def estimate_one_step_delta_after_exit(room, explored, hp, pressure, coins, items):
    """
    发现撤离点后，估计继续探索一格相对立即撤离的保守边际价值。
    该函数只计算“一格后若存活即可撤离”的保守策略。
    """
    frontier = get_frontier(explored)

    if not frontier:
        return -float("inf")

    value_now = coins + sum(items)
    failure_keep = max(items) if items else 0.0

    expected_one_room_reward = 6.3
    risks = posterior_probs(room, explored)

    best_delta = -float("inf")

    for a in frontier:
        p_mine = risks.get(a, 20 / 99)

        hp_after_pressure = hp - (2 if pressure >= 80 else 0)

        if hp_after_pressure <= 0:
            death_probability = 1.0
        elif hp_after_pressure - 30 <= 0:
            death_probability = p_mine
        else:
            death_probability = 0.0

        delta = (
            (1 - death_probability) * expected_one_room_reward
            - death_probability * (value_now - failure_keep)
        )

        best_delta = max(best_delta, delta)

    return best_delta


def simulate_once(
    seed,
    policy="minrisk",
    n=10,
    total_mines=20,
    max_steps=200,
):
    rng = Random(seed)
    start = (0, 0)

    room = generate_map(
        rng,
        n=n,
        mines=20,
        events=10,
        chests=10,
        monsters=10,
        start=start,
    )

    explored = {start}

    hp = 100
    pressure = 0
    coins = 0
    items = []
    mines_hit = 0
    monsters_seen = 0
    steps = 0
    found_exit = False
    delta_negative_at_exit = None

    while steps < max_steps and hp > 0:
        frontier = get_frontier(explored, n)

        if not frontier:
            break

        if policy == "blind":
            chosen = rng.choice(frontier)

        elif policy == "oracle":
            safe = [c for c in frontier if room[c] != MINE]
            chosen = rng.choice(safe if safe else frontier)

        elif policy == "minrisk":
            risks = posterior_probs(
                room,
                explored,
                n=n,
                total_mines=total_mines,
            )

            min_risk = min(risks.get(c, total_mines / 99) for c in frontier)

            best = [
                c for c in frontier
                if abs(risks.get(c, total_mines / 99) - min_risk) < 1e-12
            ]

            chosen = rng.choice(best)

        else:
            raise ValueError("policy must be blind, minrisk, or oracle")

        # 压力扣血判定发生在进入未知格之前。
        if pressure >= 80:
            hp -= 2

            if hp <= 0:
                break

        # 探索未知格，增加基础压力。
        pressure = min(100, pressure + 2)
        explored.add(chosen)
        steps += 1

        if room[chosen] == MINE:
            hp -= 30
            pressure = min(100, pressure + 10)
            mines_hit += 1

        elif room[chosen] == MONSTER:
            pressure = min(100, pressure + 5)
            monsters_seen += 1
            gain_coins, gain_items = reward_for_room(room, chosen, rng)
            coins += gain_coins
            items.extend(gain_items)

        else:
            gain_coins, gain_items = reward_for_room(room, chosen, rng)
            coins += gain_coins
            items.extend(gain_items)

        if hp <= 0:
            break

        if room[chosen] == EXIT:
            found_exit = True

            delta = estimate_one_step_delta_after_exit(
                room,
                explored,
                hp,
                pressure,
                coins,
                items,
            )

            delta_negative_at_exit = delta < 0
            break

    return {
        "found_exit_alive": found_exit and hp > 0,
        "died_before_exit": not found_exit and hp <= 0,
        "hp": hp,
        "pressure": pressure,
        "steps": steps,
        "mines_hit": mines_hit,
        "monsters_seen": monsters_seen,
        "value": coins + sum(items),
        "delta_negative_at_exit": delta_negative_at_exit,
    }


def run_trials(trials=1000, policy="minrisk", seed=1):
    results = [
        simulate_once(seed + i, policy=policy)
        for i in range(trials)
    ]

    successful = [r for r in results if r["found_exit_alive"]]

    def mean(xs):
        return statistics.mean(xs) if xs else None

    return {
        "policy": policy,
        "trials": trials,
        "alive_exit_found_rate": sum(r["found_exit_alive"] for r in results) / trials,
        "death_before_exit_rate": sum(r["died_before_exit"] for r in results) / trials,
        "avg_steps_all": mean([r["steps"] for r in results]),
        "avg_steps_when_exit_found": mean([r["steps"] for r in successful]),
        "avg_mines_hit_all": mean([r["mines_hit"] for r in results]),
        "avg_mines_hit_when_exit_found": mean([r["mines_hit"] for r in successful]),
        "avg_monsters_seen_when_exit_found": mean([r["monsters_seen"] for r in successful]),
        "avg_hp_when_exit_found": mean([r["hp"] for r in successful]),
        "avg_pressure_when_exit_found": mean([r["pressure"] for r in successful]),
        "avg_value_when_exit_found": mean([r["value"] for r in successful]),
        "delta_negative_rate_at_exit": (
            sum(r["delta_negative_at_exit"] is True for r in successful)
            / len(successful)
            if successful
            else None
        ),
    }


if __name__ == "__main__":
    for policy in ("blind", "minrisk", "oracle"):
        result = run_trials(trials=1000, policy=policy, seed=42)
        print(result)
```

## 附录 B：程序输出指标解释

`alive_exit_found_rate` 表示玩家活着发现撤离点的比例。这是修正模型后的首要指标，因为在必须先找到撤离点才能撤离的规则下，若该值过低，后续撤离决策分析意义会下降。

`death_before_exit_rate` 表示玩家在发现撤离点前死亡的比例。它衡量的是撤离点未发现阶段的强制探索风险。

`avg_steps_when_exit_found` 表示玩家活着发现撤离点时平均探索的新房间数。它不应简单等同于 50，因为这是条件在“活着发现撤离点”后的样本。

`avg_hp_when_exit_found` 和 `avg_pressure_when_exit_found` 表示发现撤离点时的状态。如果 HP 经常低于 32，则发现撤离点后继续探索会变得非常危险；如果 HP 大多高于 32，则继续探索一格通常仍有空间。

`delta_negative_rate_at_exit` 表示发现撤离点后，继续探索一格的保守边际价值为负的比例。它近似对应：

[
P(\Delta_{\text{one-step}}<0\mid \text{alive at exit discovery}).
]

`blind` 策略代表不使用数字信息的玩家。`oracle` 策略代表全知避雷上界。`minrisk` 策略代表利用数字信息选择最低雷率候选格的保守玩家。三者对比可以判断当前配置究竟是在随机探索下过难，还是在扫雷信息使用下可成立。

## 附录 C：程序限制

该程序并不是完整理性玩家求解器。完整非全知理性玩家应在每一步最大化最终期望价值，而不是只选择最低雷率格。他需要同时考虑雷率、房间收益、撤离点概率、信息价值、当前背包价值、失败保底、HP、压力和未来路径。该问题属于有限但状态空间巨大的部分可观测最优决策问题，严格求解需要动态规划、蒙特卡洛树搜索或更复杂的前沿枚举。

因此，本文程序主要用于数值验证和趋势比较，而不是最终证明。它适合比较不同 Demo 参数，例如 15 雷、18 雷、20 雷、单撤离点、双撤离点、是否起点保护、是否加入压力扣血等配置。
