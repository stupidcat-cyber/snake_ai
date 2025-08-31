
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>

// --- 从示例代码和文档中提取的常量与结构体 ---

constexpr int MAXN = 40; // 游戏区域宽度
constexpr int MAXM = 30; // 游戏区域高度
constexpr int MAX_TICKS = 256;
constexpr int MYID = 2024201552; // !!! 此处替换为你的学号 !!!

struct Point {
    int y, x;
    bool operator==(const Point& other) const {
        return y == other.y && x == other.x;
    }
    bool operator<(const Point& other) const { // 用于std::set和std::map
        if (y != other.y) return y < other.y;
        return x < other.x;
    }
};

struct Item {
    Point pos;
    int value;
    int lifetime;
};

struct Snake {
    int id;
    int length;
    int score;
    int direction;
    int shield_cd;
    int shield_time;
    bool has_key;
    std::vector<Point> body;
    const Point& get_head() const { return body.front(); }
};

struct SafeZoneBounds {
    int x_min, y_min, x_max, y_max;
};

struct GameState {
    int remaining_ticks;
    std::vector<Item> items;
    std::vector<Snake> snakes;
    SafeZoneBounds current_safe_zone;
    int self_idx;
    const Snake& get_self() const { return snakes[self_idx]; }
};

// --- AI 决策逻辑 ---

// 方向常量: 0:左, 1:上, 2:右, 3:下
const int DX[] = {-1, 0, 1, 0};
const int DY[] = {0, -1, 0, 1};
const int OPPOSITE_DIR[] = {2, 3, 0, 1};

// 检查一个点是否在地图范围内
bool is_in_bounds(const Point& p) {
    return p.x >= 0 && p.x < MAXN && p.y >= 0 && p.y < MAXM;
}

bool is_deadly(const Point& p, const GameState& s, bool consider_other_snake_heads = true) {
    const auto& self = s.get_self();
    
    // 1. 撞墙
    if (!is_in_bounds(p)) {
        return true;
    }
    
    // 2. 离开安全区 (没有护盾)
    if (self.shield_time <= 0 &&
        (p.x <= s.current_safe_zone.x_min || p.x >= s.current_safe_zone.x_max ||
         p.y <= s.current_safe_zone.y_min || p.y >= s.current_safe_zone.y_max)) {
        return true;
    }
    
    // 3. 撞到陷阱
    for (const auto& item : s.items) {
        if (p == item.pos && item.value == -2) {
            return true;
        }
    }
    
    // 4. 检查其他蛇的身体（不包括蛇头，因为蛇头会移动）
    int obstacle_count = 0; // 计算p周围有多少个障碍
    
    for (const auto& snake : s.snakes) {
        // 拥有护盾时，不会撞到其他蛇的身体
        if (self.shield_time > 0 && snake.id != MYID) {
            continue;
        }
        
        // 检查蛇的身体（不包括尾巴，因为尾巴会移动）
        for (size_t i = 0; i < snake.body.size(); ++i) {
            // 跳过自己的蛇身（因为蛇可以掉头）
            if (snake.id == MYID) continue;
            
            // 跳过尾巴（会移动）
            if (i == snake.body.size() - 1) continue;
            
            if (p == snake.body[i]) {
                return true; // 直接撞到身体
            }
            
            // 检查周围四个方向是否有障碍物
            for (int dir = 0; dir < 4; ++dir) {
                Point neighbor = {p.y + DY[dir], p.x + DX[dir]};
                if (neighbor == snake.body[i]) {
                    obstacle_count++;
                }
            }
        }
    }

    // 5. 预测其他蛇的头部位置
    if (consider_other_snake_heads) {
        for (const auto& snake : s.snakes) {
            if (snake.id == MYID) continue;
            
            Point other_head = snake.get_head();
            for (int other_dir = 0; other_dir < 4; ++other_dir) {
                Point other_next_pos = {other_head.y + DY[other_dir], other_head.x + DX[other_dir]};
                if (p == other_next_pos) {
                    return true;
                }
                
                // 检查周围是否有其他蛇的预测头部
                for (int dir = 0; dir < 4; ++dir) {
                    Point neighbor = {p.y + DY[dir], p.x + DX[dir]};
                    if (neighbor == other_next_pos) {
                        obstacle_count++;
                    }
                }
            }
        }
    }
    
    // 6. 检查墙壁和安全区边界（作为障碍物）
    for (int dir = 0; dir < 4; ++dir) {
        Point neighbor = {p.y + DY[dir], p.x + DX[dir]};
        
        // 检查是否出界
        if (!is_in_bounds(neighbor)) {
            obstacle_count++;
            continue;
        }
        
        // 检查是否离开安全区（没有护盾时）
        if (self.shield_time <= 0 &&
            (neighbor.x < s.current_safe_zone.x_min || neighbor.x > s.current_safe_zone.x_max ||
             neighbor.y < s.current_safe_zone.y_min || neighbor.y > s.current_safe_zone.y_max)) {
            obstacle_count++;
        }
    }
    
    // 7. 检查陷阱（作为障碍物）
    for (const auto& item : s.items) {
        if (item.value == -2) { // 陷阱
            for (int dir = 0; dir < 4; ++dir) {
                Point neighbor = {p.y + DY[dir], p.x + DX[dir]};
                if (neighbor == item.pos) {
                    obstacle_count++;
                }
            }
        }
    }
    
    // 调试输出
    // std::cerr << "Point (" << p.y << ", " << p.x << ") has " << obstacle_count << " obstacles around" << std::endl;
    
    // 如果周围有3个或以上障碍物，则认为走投无路
    if (obstacle_count >= 3) {
        // std::cerr << "DEAD END detected at (" << p.y << ", " << p.x << ")" << std::endl;
        return true;
    }
    
    return false;
}

// 评估一个点周围的安全空间 (BFS)
int calculate_safe_space(const Point& start_pos, const GameState& s, int max_depth = 10) {
    std::queue<Point> q;
    std::set<Point> visited;
    q.push(start_pos);
    visited.insert(start_pos);
    int safe_count = 0;

    // 模拟一个临时的游戏状态，将当前蛇头位置视为障碍物，以计算从start_pos开始的自由空间
    GameState temp_s = s;
    // 将自己的身体也视为障碍物，除了尾巴（如果长度不变）
    std::set<Point> obstacles;
    for(const auto& snake : s.snakes) {
        if(snake.id == MYID) continue;
        for(size_t i = 0; i < snake.body.size(); ++i) {
            // 自己的尾巴在下一刻会空出来，除非吃到增长豆
            if (snake.id == MYID && i == snake.body.size() - 1) continue;
            // 其他蛇的尾巴在下一刻会空出来，除非吃到增长豆
            if (snake.id != MYID && i == snake.body.size() - 1) continue;
            obstacles.insert(snake.body[i]);
        }
    }




    while (!q.empty() && safe_count < max_depth) {
        Point current = q.front();
        q.pop();
        safe_count++;

        for (int dir = 0; dir < 4; ++dir) {
            Point next = {current.y + DY[dir], current.x + DX[dir]};

            // 检查是否在地图范围内
            if (!is_in_bounds(next)) continue;
            // 检查是否在安全区内
            if (s.get_self().shield_time <= 0 &&
                (next.x < s.current_safe_zone.x_min || next.x > s.current_safe_zone.x_max ||
                 next.y < s.current_safe_zone.y_min || next.y > s.current_safe_zone.y_max)) {
                continue;
            }
            // 检查是否撞到陷阱
            bool is_trap = false;
            for(const auto& item : s.items) {
                if (next == item.pos && item.value == -2) {
                    is_trap = true;
                    break;
                }
            }
            if (is_trap) continue;

            // 检查是否撞到障碍物 (包括其他蛇的身体和预测的头部)
            bool is_obstacle = false;
            if (obstacles.count(next)) {
                is_obstacle = true;
            } else {
                // 预测其他蛇的头部位置
                for (const auto& snake : s.snakes) {
                    if (snake.id == MYID) continue;
                    Point other_head = snake.get_head();
                    for (int other_dir = 0; other_dir < 4; ++other_dir) {
                        if (snake.length > 1 && other_dir == OPPOSITE_DIR[snake.direction]) continue;
                        Point other_next_pos = {other_head.y + DY[other_dir], other_head.x + DX[other_dir]};
                        if (next == other_next_pos) {
                            is_obstacle = true;
                            break;
                        }
                    }
                    if (is_obstacle) break;
                }
            }
            if (is_obstacle) continue;

            if (visited.find(next) == visited.end()) {
                visited.insert(next);
                q.push(next);
            }
        }
    }
    return safe_count;
}

// 评估一个目标点的分数 (改进版)
double evaluate_target(const Point& target, const Item& item, const Snake& self, const GameState& s) {
    const auto& head = self.get_head();
    int dist = std::abs(head.y - target.y) + std::abs(head.x - target.x);
    if (dist == 0) dist = 1; // 避免除以零

    double score = 0;
    // 根据物品类型进行评分
    if (item.value > 0) { // 普通食物
        score = (double)item.value * 100.0 / dist; // 增加食物的吸引力
        // 引入可达性因子：评估到达食物的路径安全性
        // 简单的可达性评估：检查食物周围是否有足够的安全空间
        int safe_space_around_food = calculate_safe_space(target, s, 5);
        score += safe_space_around_food * 10; // 安全空间越大，食物吸引力越高
    } else if (item.value == -1) { // 增长豆
        // 动态调整增长豆优先级：
        // 1. 蛇很短（例如，长度小于10），需要快速增长以占据优势
        // 2. 周围空间充足，不会因为增长而立即被困
        // 3. 场上没有更高价值的普通食物
        int safe_space_around_bean = calculate_safe_space(target, s, 5); // 评估增长豆周围的安全空间
        bool has_high_value_food = false;
        for (const auto& other_item : s.items) {
            if (other_item.value > 0 && other_item.value >= 3) { // 存在高分食物
                has_high_value_food = true;
                break;
            }
        }

        if (self.length < 10 && safe_space_around_bean > 10 && !has_high_value_food) { 
            score = 80.0 / dist; // 提高增长豆的优先级
        } else if (self.length < 15 && safe_space_around_bean > 5) { 
            score = 30.0 / dist; // 降低增长豆的优先级
        } else {
            score = -1e9; // 避免在不安全或过长时吃增长豆
        }
    }
    // 陷阱 (-2) 和其他负值物品的评分为负，AI会主动避开
    else if (item.value == -2) {
        score = -1e12; // 极力避免陷阱
    }
    else if (item.value == -3) { // 钥匙
        score = 2000.0 / dist;
    }
    else if (item.value == -5) { // 宝箱
        if(self.has_key) {
            score = 2e5 / dist*1.0;
        }
        else score = -1e12;
    }
    return score;
}

// --- 主程序 ---

void read_game_state(GameState& s) {
    std::cin >> s.remaining_ticks;
    int item_count;
    std::cin >> item_count;
    s.items.resize(item_count);
    for (int i = 0; i < item_count; ++i) {
        std::cin >> s.items[i].pos.y >> s.items[i].pos.x >> s.items[i].value >> s.items[i].lifetime;
    }

    int snake_count;
    std::cin >> snake_count;
    s.snakes.resize(snake_count);
    std::map<int, int> id2idx;
    for (int i = 0; i < snake_count; ++i) {
        auto& sn = s.snakes[i];
        std::cin >> sn.id >> sn.length >> sn.score >> sn.direction >> sn.shield_cd >> sn.shield_time;
        sn.body.resize(sn.length);
        for (int j = 0; j < sn.length; ++j) {
            std::cin >> sn.body[j].y >> sn.body[j].x;
        }
        if (sn.id == MYID) {
            s.self_idx = i;
        }
        id2idx[sn.id] = i;
    }
    
    // 跳过宝箱和钥匙信息，本AI不使用
    int chest_count, key_count;
    std::cin >> chest_count;
    for(int i=0; i<chest_count; ++i) { int y,x,score; std::cin >> y >> x >> score; }
    std::cin >> key_count;
    for(int i=0; i<key_count; ++i) { int y,x,id,time; std::cin >> y >> x >> id >> time; }


    std::cin >> s.current_safe_zone.x_min >> s.current_safe_zone.y_min >> s.current_safe_zone.x_max >> s.current_safe_zone.y_max;
    
    // 跳过未来的安全区信息
    int next_tick, final_tick;
    int nx_min, ny_min, nx_max, ny_max;
    int fx_min, fy_min, fx_max, fy_max;
    std::cin >> next_tick >> nx_min >> ny_min >> nx_max >> ny_max;
    std::cin >> final_tick >> fx_min >> fy_min >> fx_max >> fy_max;
}

int main() {
    // 关闭C++标准流与C标准流的同步，加快IO速度
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    GameState current_state;
    read_game_state(current_state);

    const auto& self = current_state.get_self();
    const auto& head = self.get_head();

    // 读取上回合存储的记忆
    int last_decision = -1;
    if (current_state.remaining_ticks < MAX_TICKS -1) { // 只有在非第一回合才读取
        std::cin >> last_decision;
    }

    // 1. 确定最佳目标物品
    Item best_target_item;
    double max_item_score = -1e15; // 使用一个非常小的负数作为初始值
    bool has_target = false;

    for (const auto& item : current_state.items) {
        double score = evaluate_target(item.pos, item, self, current_state);
        if (score > max_item_score) {
            max_item_score = score;
            best_target_item = item;
            has_target = true;
        }
    }

    // 2. 决策过程：根据目标和安全情况选择方向
    int best_dir = -1;
    double best_dir_score = -1e15; // 使用一个非常小的负数作为初始值

    for (int dir = 0; dir < 4; ++dir) {
        // 避免回头
        if (self.length > 1 && dir == OPPOSITE_DIR[self.direction]) {
            continue;
        }

        Point next_pos = {head.y + DY[dir], head.x + DX[dir]};

        // 安全性检查：使用改进后的is_deadly，考虑其他蛇的头部
        if (is_deadly(next_pos, current_state, true)) {
            continue;
        }

        // 评估这个方向的得分
        double current_dir_score = 0;
        if (has_target) {
            int dist_to_target = std::abs(next_pos.y - best_target_item.pos.y) + std::abs(next_pos.x - best_target_item.pos.x);
            current_dir_score = max_item_score - dist_to_target; // 目标物品分数越高，离目标越近，分数越高

            // 初步的竞争分析：如果其他蛇离目标更近，降低该方向的吸引力
            for (const auto& other_snake : current_state.snakes) {
                if (other_snake.id == MYID) continue;
                int other_dist_to_target = std::abs(other_snake.get_head().y - best_target_item.pos.y) + std::abs(other_snake.get_head().x - best_target_item.pos.x);
                if (other_dist_to_target < dist_to_target) {
                    current_dir_score -= (dist_to_target - other_dist_to_target) * 50; // 距离越近，惩罚越大
                }
            }
        } else {
            // 如果没有目标，就选择安全空间最大的方向
            current_dir_score = calculate_safe_space(next_pos, current_state, 10); // 评估周围安全空间
        }
        
        // 优先选择安全空间更大的方向，作为次要评估标准
        current_dir_score += calculate_safe_space(next_pos, current_state, 5) * 0.1; // 乘以一个小数，避免主次颠倒

        if (current_dir_score > best_dir_score) {
            best_dir_score = current_dir_score;
            best_dir = dir;
        }
    }

    // 3. 如果所有方向都危险，尝试寻找任何一个安全的备用方向 (更智能的无路可走策略)
    if (best_dir == -1) {
        int max_safe_space = -1;
        int fallback_dir = -1;
        for (int dir = 0; dir < 4; ++dir) {
            // 避免回头
            if (self.length > 1 && dir == OPPOSITE_DIR[self.direction]) continue;
            Point next_pos = {head.y + DY[dir], head.x + DX[dir]};
            
            // 此时不考虑其他蛇的头部，只考虑墙壁、自己的身体和陷阱
            // 这是一个“逃生”模式，优先寻找能活下去的方向
            if (!is_deadly(next_pos, current_state, false)) {
                int safe_space = calculate_safe_space(next_pos, current_state, 5); // 评估这个方向的安全空间
                if (safe_space > max_safe_space) {
                    max_safe_space = safe_space;
                    fallback_dir = dir;
                }
            }
        }
        best_dir = fallback_dir;
    }
    
    // 4. 如果实在无路可走（比如被包围），随机选择一个方向（听天由命）
    if (best_dir == -1) {
        best_dir = 0; // 默认向左
        // 尝试选择一个非回头方向
        for (int dir = 0; dir < 4; ++dir) {
             if (self.length > 1 && dir == OPPOSITE_DIR[self.direction]) continue;
             best_dir = dir;
             break;
        }
    }

    // 输出决策并记录到Memory
    std::cout << best_dir << std::endl;
    std::cout << best_dir << std::endl; // 将本次决策作为记忆传递给下一回合

    return 0;
}


